#include "computerseeker.h"
#include "computermanager.h"
#include <QTimer>
#include <QUrl>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUdpSocket>

ComputerSeeker::ComputerSeeker(ComputerManager *manager, QString computerName, QObject *parent)
    : QObject(parent), m_ComputerManager(manager), m_ComputerName(computerName),
      m_TimeoutTimer(new QTimer(this))
{
    // If we know this computer, send a WOL packet to wake it up in case it is asleep.
    NvComputer* matchingComputer = findMatchingComputer();
    if (matchingComputer) {
        matchingComputer->wake();
    }

    m_TimeoutTimer->setSingleShot(true);
    connect(m_TimeoutTimer, &QTimer::timeout,
            this, &ComputerSeeker::onTimeout);
    connect(m_ComputerManager, &ComputerManager::computerStateChanged,
            this, &ComputerSeeker::onComputerUpdated);
}

void ComputerSeeker::start(int timeout)
{
    m_TimeoutTimer->start(timeout);
    const auto target = QUrl::fromUserInput("moonlight://" + m_ComputerName);
    const QHostAddress targetAddress(target.host());
    if (!targetAddress.isNull() && target.port(-1) < 0) {
        auto *announcements = new QUdpSocket(this);
        if (announcements->bind(QHostAddress::AnyIPv4, 47996,
                                QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            connect(announcements, &QUdpSocket::readyRead, this, [this, announcements, targetAddress]() {
                while (announcements->hasPendingDatagrams()) {
                    QByteArray payload;
                    payload.resize(static_cast<int>(announcements->pendingDatagramSize()));
                    QHostAddress sender;
                    announcements->readDatagram(payload.data(), payload.size(), &sender);
                    if (sender != targetAddress) continue;
                    const auto message = QJsonDocument::fromJson(payload).object();
                    const int port = message.value("MoonlightHttpPort").toInt();
                    if (message.value("Protocol").toString() != QStringLiteral("WINHANCED_COMPANION_V1") ||
                        port < 1 || port > 65535 || m_HostSearchStarted) continue;
                    m_ComputerName = targetAddress.toString() + ':' + QString::number(port);
                    announcements->close();
                    startHostSearch();
                    return;
                }
            });
            QTimer::singleShot(1500, this, [this, announcements]() {
                announcements->close();
                startHostSearch();
            });
            return;
        }
        announcements->deleteLater();
    }
    startHostSearch();
}

void ComputerSeeker::startHostSearch()
{
    if (m_HostSearchStarted) return;
    m_HostSearchStarted = true;
    // Seek desired computer by both connecting to it directly (this may fail
    // if m_ComputerName is UUID, or the name that doesn't resolve to an IP
    // address) and by polling it using mDNS, hopefully one of these methods
    // would find the host
    if (!findMatchingComputer()) {
        m_ComputerManager->addNewHostManually(m_ComputerName);
    }
    m_ComputerManager->startPolling();
}

void ComputerSeeker::onComputerUpdated(NvComputer *computer)
{
    if (!m_TimeoutTimer->isActive()) {
        return;
    }
    if (matchComputer(computer) && isOnline(computer)) {
        m_ComputerManager->stopPollingAsync();
        m_TimeoutTimer->stop();
        emit computerFound(computer);
    }
}

bool ComputerSeeker::matchComputer(NvComputer *computer) const
{
    QString value = m_ComputerName.toLower();

    // A saved manual/LAN address can belong to an obsolete server UUID after a
    // reinstall. An explicit IP must match the address that actually responded.
    const auto target = QUrl::fromUserInput("moonlight://" + m_ComputerName);
    const QHostAddress targetAddress(target.host());
    if (!targetAddress.isNull()) {
        return QHostAddress(computer->activeAddress.address()) == targetAddress &&
               computer->activeAddress.port() == target.port(DEFAULT_HTTP_PORT);
    }

    if (computer->name.toLower() == value || computer->uuid.toLower() == value) {
        return true;
    }

    const auto uniqueAddresses = computer->uniqueAddresses();
    for (const NvAddress& addr : uniqueAddresses) {
        if (addr.address().toLower() == value || addr.toString().toLower() == value) {
            return true;
        }
    }

    return false;
}

NvComputer* ComputerSeeker::findMatchingComputer() const
{
    const auto computers = m_ComputerManager->getComputers();
    for (NvComputer* computer : computers) {
        if (this->matchComputer(computer)) {
            return computer;
        }
    }

    return nullptr;
}

bool ComputerSeeker::isOnline(NvComputer *computer) const
{
    return computer->state == NvComputer::CS_ONLINE;
}

void ComputerSeeker::onTimeout()
{
    m_TimeoutTimer->stop();
    m_ComputerManager->stopPollingAsync();
    emit errorTimeout();
}
