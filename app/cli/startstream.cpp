#include "startstream.h"
#include "backend/computermanager.h"
#include "backend/computerseeker.h"
#include "streaming/session.h"

#include <QCoreApplication>
#include <QPointer>
#include <QTimer>
#include <QFutureWatcher>
#include <QPromise>
#include <QThreadPool>
#include <QUrl>
#include <memory>

#define COMPUTER_SEEK_TIMEOUT 30000
#define APP_SEEK_TIMEOUT 10000

namespace CliStartStream
{

enum State {
    StateInit,
    StateSeekComputer,
    StateSeekApp,
    StateStartSession,
    StateFailure,
};

class Event
{
public:
    enum Type {
        AppQuitCompleted,
        AppQuitRequested,
        ComputerFound,
        ComputerUpdated,
        Executed,
        Timedout,
    };

    Event(Type type)
        : type(type), computerManager(nullptr), computer(nullptr) {}

    Type type;
    ComputerManager *computerManager;
    NvComputer *computer;
    QString errorMessage;
};

class LauncherPrivate
{
    Q_DECLARE_PUBLIC(Launcher)

public:
    LauncherPrivate(Launcher *q) : q_ptr(q) {}

    ~LauncherPrivate() { stopAppPolling(); }

    void stopAppPolling()
    {
        if (m_AppPolling && m_ComputerManager) {
            m_ComputerManager->stopPollingAsync();
        }
        m_AppPolling = false;
    }

    void setState(State state)
    {
        m_State = state;
        if (state == StateStartSession || state == StateFailure) {
            stopAppPolling();
        }
    }

    void startDirect(bool quitExisting = false)
    {
        Q_Q(Launcher);
        if (m_DirectResolving) return;
        m_DirectResolving = true;
        struct Result { QSharedPointer<NvComputer> computer; QString error; };
        auto promise = std::make_shared<QPromise<Result>>();
        auto watcher = new QFutureWatcher<Result>(q);
        q->connect(watcher, &QFutureWatcher<Result>::finished, q, [this, q, watcher]() {
            const auto result = watcher->result();
            watcher->deleteLater();
            m_DirectResolving = false;
            if (!result.error.isEmpty()) {
                setState(StateFailure);
                emit q->failed(result.error);
                return;
            }
            m_DirectComputer = result.computer;
            m_Computer = m_DirectComputer.data();
            if (m_Computer->pairState != NvComputer::PS_PAIRED) {
                setState(StateFailure);
                emit q->failed(QObject::tr("The server at %1 has not paired this Moonlight client.").arg(m_ComputerName));
                return;
            }
            NvApp app;
            app.id = m_DirectAppId;
            app.name = m_AppName;
            app.hdrSupported = (m_Computer->serverCodecModeSupport & SCM_MASK_10BIT) != 0;
            if (!isNotStreaming() && !isStreamingApp(app)) {
                setState(StateSeekApp);
                emit q->appQuitRequired(QObject::tr("another application"));
                return;
            }
            setState(StateStartSession);
            qInfo() << "Direct launch: using supplied app ID" << app.id << "at" << m_Computer->activeAddress.toString()
                    << "without saved-desktop selection or application-list lookup";
            auto session = new Session(m_Computer, app, m_Preferences);
            QObject::connect(session, &QObject::destroyed, [computer = m_DirectComputer]() {});
            emit q->sessionCreated(app.name, session);
        });
        watcher->setFuture(promise->future());
        promise->start();
        const auto target = QUrl::fromUserInput("moonlight://" + m_ComputerName);
        const NvAddress address(target.host(), target.port(DEFAULT_HTTP_PORT));
        const auto fingerprint = m_DirectFingerprint;
        const auto httpsPort = m_DirectHttpsPort;
        QThreadPool::globalInstance()->start(QRunnable::create([promise, address, fingerprint, httpsPort, quitExisting]() {
            Result result;
            try {
                NvHTTP http(address, httpsPort, QSslCertificate());
                QString info = http.getDirectServerInfo(fingerprint);
                if (quitExisting && NvHTTP::getXmlString(info, "PairStatus") == "1") {
                    http.quitApp();
                    info = http.getDirectServerInfo(fingerprint);
                }
                result.computer.reset(new NvComputer(http, info));
                // The caller supplies the reachable HTTPS port, including custom forwarding.
                result.computer->activeHttpsPort = httpsPort;
            }
            catch (const GfeHttpResponseException& e) { result.error = e.toQString(); }
            catch (const QtNetworkReplyException& e) { result.error = e.toQString(); }
            catch (const std::exception& e) { result.error = QString::fromUtf8(e.what()); }
            promise->addResult(result);
            promise->finish();
        }));
    }

    void handleEvent(Event event)
    {
        Q_Q(Launcher);
        Session* session;
        NvApp app;

        switch (event.type) {
        // Occurs when CliStartStreamSegue becomes visible and the UI calls launcher's execute()
        case Event::Executed:
            if (m_State == StateInit) {
                m_State = StateSeekComputer;
                m_ComputerManager = event.computerManager;

                if (m_DirectAppId != 0) {
                    emit q->searchingComputer();
                    startDirect();
                    break;
                }

                // ComputerSeeker releases its polling reference when the PC is found.
                // Keep a separate reference until its application list is available.
                m_ComputerManager->startPolling();
                m_AppPolling = true;

                m_ComputerSeeker = new ComputerSeeker(m_ComputerManager, m_ComputerName, q);
                q->connect(m_ComputerSeeker, &ComputerSeeker::computerFound,
                           q, &Launcher::onComputerFound);
                q->connect(m_ComputerSeeker, &ComputerSeeker::errorTimeout,
                           q, &Launcher::onTimeout);
                m_ComputerSeeker->start(COMPUTER_SEEK_TIMEOUT);

                q->connect(m_ComputerManager, &ComputerManager::computerStateChanged,
                           q, &Launcher::onComputerUpdated);
                q->connect(m_ComputerManager, &ComputerManager::quitAppCompleted,
                           q, &Launcher::onQuitAppCompleted);

                emit q->searchingComputer();
            }
            break;
        // Occurs when searched computer is found
        case Event::ComputerFound:
            if (m_State == StateSeekComputer) {
                if (event.computer->pairState == NvComputer::PS_PAIRED) {
                    setState(StateSeekApp);
                    m_Computer = event.computer;
                    m_TimeoutTimer->start(APP_SEEK_TIMEOUT);
                    emit q->searchingApp();
                    Event updated(Event::ComputerUpdated);
                    updated.computer = m_Computer;
                    handleEvent(updated);
                } else {
                    setState(StateFailure);
                    QString msg = QObject::tr("Computer %1 has not been paired. "
                                              "Please open Moonlight to pair before streaming.")
                            .arg(event.computer->name);
                    emit q->failed(msg);
                }
            }
            break;
        // Occurs when a computer is updated
        case Event::ComputerUpdated:
            if (m_State == StateSeekApp && event.computer == m_Computer) {
                int index = getAppIndex();
                if (-1 != index) {
                    app = m_Computer->appList[index];
                    m_TimeoutTimer->stop();
                    if (isNotStreaming() || isStreamingApp(app)) {
                        setState(StateStartSession);
                        session = new Session(m_Computer, app, m_Preferences);
                        emit q->sessionCreated(app.name, session);
                    } else {
                        emit q->appQuitRequired(getCurrentAppName());
                    }
                }
            }
            break;
        // Occurs when there was another app running on computer and user accepted quit
        // confirmation dialog
        case Event::AppQuitRequested:
            if (m_State == StateSeekApp) {
                if (m_DirectAppId != 0) { startDirect(true); break; }
                m_ComputerManager->quitRunningApp(m_Computer);
            }
            break;
        // Occurs when the previous app quit has been completed, handles quitting errors if any
        // happened. ComputerUpdated event's handler handles session start when previous app has
        // quit.
        case Event::AppQuitCompleted:
            if (m_State == StateSeekApp && !event.errorMessage.isEmpty()) {
                setState(StateFailure);
                emit q->failed(QObject::tr("Quitting app failed, reason: %1").arg(event.errorMessage));
            }
            break;
        // Occurs when computer or app search timed out
        case Event::Timedout:
            if (m_State == StateSeekComputer) {
                setState(StateFailure);
                emit q->failed(QObject::tr("Failed to connect to %1").arg(m_ComputerName));
            }
            if (m_State == StateSeekApp) {
                setState(StateFailure);
                emit q->failed(QObject::tr("Failed to find application %1").arg(m_AppName));
            }
            break;
        }
    }

    int getAppIndex() const
    {
        for (int i = 0; i < m_Computer->appList.length(); i++) {
            if (m_Computer->appList[i].name.toLower() == m_AppName.toLower()) {
                return i;
            }
        }
        return -1;
    }

    bool isNotStreaming() const
    {
        return m_Computer->currentGameId == 0;
    }

    bool isStreamingApp(NvApp app) const
    {
        return m_Computer->currentGameId == app.id;
    }

    QString getCurrentAppName() const
    {
        for (const NvApp& app : m_Computer->appList) {
            if (m_Computer->currentGameId == app.id) {
                return app.name;
            }
        }
        return "<UNKNOWN>";
    }

    Launcher *q_ptr;
    QString m_ComputerName;
    QString m_AppName;
    int m_DirectAppId = 0;
    QByteArray m_DirectFingerprint;
    quint16 m_DirectHttpsPort = 0;
    bool m_DirectResolving = false;
    QSharedPointer<NvComputer> m_DirectComputer;
    StreamingPreferences *m_Preferences;
    QPointer<ComputerManager> m_ComputerManager;
    bool m_AppPolling = false;
    ComputerSeeker *m_ComputerSeeker;
    NvComputer *m_Computer;
    State m_State;
    QTimer *m_TimeoutTimer;
};

Launcher::Launcher(QString computer, QString app,
                   StreamingPreferences* preferences, QObject *parent)
    : QObject(parent),
      m_DPtr(new LauncherPrivate(this))
{
    Q_D(Launcher);
    d->m_ComputerName = computer;
    d->m_AppName = app;
    d->m_Preferences = preferences;
    d->m_State = StateInit;
    d->m_TimeoutTimer = new QTimer(this);
    d->m_TimeoutTimer->setSingleShot(true);
    connect(d->m_TimeoutTimer, &QTimer::timeout,
            this, &Launcher::onTimeout);
}

Launcher::~Launcher()
{
}

void Launcher::setDirectTarget(int appId, QByteArray serverFingerprint, quint16 httpsPort)
{
    Q_D(Launcher);
    Q_ASSERT(d->m_State == StateInit);
    d->m_DirectAppId = appId;
    d->m_DirectFingerprint = std::move(serverFingerprint);
    d->m_DirectHttpsPort = httpsPort;
}

void Launcher::execute(ComputerManager *manager)
{
    Q_D(Launcher);
    Event event(Event::Executed);
    event.computerManager = manager;
    d->handleEvent(event);
}

void Launcher::quitRunningApp()
{
    Q_D(Launcher);
    Event event(Event::AppQuitRequested);
    d->handleEvent(event);
}

bool Launcher::isExecuted() const
{
    Q_D(const Launcher);
    return d->m_State != StateInit;
}

void Launcher::onComputerFound(NvComputer *computer)
{
    Q_D(Launcher);
    Event event(Event::ComputerFound);
    event.computer = computer;
    d->handleEvent(event);
}

void Launcher::onComputerUpdated(NvComputer *computer)
{
    Q_D(Launcher);
    Event event(Event::ComputerUpdated);
    event.computer = computer;
    d->handleEvent(event);
}

void Launcher::onTimeout()
{
    Q_D(Launcher);
    Event event(Event::Timedout);
    d->handleEvent(event);
}

void Launcher::onQuitAppCompleted(QVariant error)
{
    Q_D(Launcher);
    Event event(Event::AppQuitCompleted);
    event.errorMessage = error.toString();
    d->handleEvent(event);
}

}
