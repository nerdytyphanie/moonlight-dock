#include "settings/streamingpreferences.h"
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

// These tests exercise settings persistence without inspecting the display.
namespace WMUtils {
bool isRunningWayland() { return false; }
bool isGpuSlow() { return false; }
}

class VrrPreferencesTest : public QObject
{
    Q_OBJECT
    QTemporaryDir directory;
private slots:
    void initTestCase()
    {
        QVERIFY(directory.isValid());
        QCoreApplication::setOrganizationName("MoonlightVrrSettingsTest");
        QCoreApplication::setApplicationName("IsolatedPreferences");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, directory.path());
    }
    void init() { QSettings().clear(); }
    void migration()
    {
        auto* prefs = StreamingPreferences::get();
        prefs->reload();
        QCOMPARE(prefs->vrrBufferPerMille(), 1000);
        for (int mode : {0, 1, 2}) {
            QSettings().setValue("vrrlatencymode", mode);
            prefs->reload();
            const auto expected = VrrTimingOptions::preset(mode);
            QCOMPARE(prefs->vrrBufferPerMille(), expected.bufferPerMille);
            QCOMPARE(prefs->vrrTargetHundredths(), expected.targetHundredths);
            QCOMPARE(prefs->vrrHistorySeconds(), expected.historySeconds);
            QCOMPARE(prefs->vrrToleranceUs(), expected.toleranceUs);
        }
    }
    void customRoundTripAndPresetReset()
    {
        auto* prefs = StreamingPreferences::get();
        prefs->reload();
        prefs->setVrrBufferPerMille(750);
        prefs->setVrrTargetHundredths(9725);
        prefs->setVrrHistorySeconds(30);
        prefs->setVrrToleranceUs(1500);
        prefs->save();
        prefs->applyVrrPreset(0);
        prefs->reload();
        QCOMPARE(prefs->vrrBufferPerMille(), 750);
        QCOMPARE(prefs->vrrTargetHundredths(), 9725);
        QCOMPARE(prefs->vrrHistorySeconds(), 30);
        QCOMPARE(prefs->vrrToleranceUs(), 1500);
        prefs->applyVrrPreset(2);
        prefs->save();
        prefs->reload();
        QCOMPARE(prefs->vrrBufferPerMille(), 500);
        QCOMPARE(prefs->vrrTargetHundredths(), 9900);
        QCOMPARE(prefs->vrrHistorySeconds(), 60);
        QCOMPARE(prefs->vrrToleranceUs(), 500);
    }
    void launchPresetOverridesSavedTiming_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<bool>("custom");
        QTest::addColumn<bool>("vrrEnabled");
        for (int mode : {0, 1, 2}) {
            for (bool custom : {false, true}) {
                for (bool enabled : {false, true}) {
                    const auto name = QString("mode%1-custom%2-vrr%3").arg(mode).arg(custom).arg(enabled).toLatin1();
                    QTest::newRow(name.constData()) << mode << custom << enabled;
                }
            }
        }
    }
    void launchPresetOverridesSavedTiming()
    {
        QFETCH(int, mode);
        QFETCH(bool, custom);
        QFETCH(bool, vrrEnabled);
        auto* prefs = StreamingPreferences::get();
        prefs->reload();
        prefs->applyVrrPreset(StreamingPreferences::VLM_BALANCED);
        prefs->enableVrr = vrrEnabled;
        if (custom) {
            prefs->setVrrBufferPerMille(750);
            prefs->setVrrTargetHundredths(9725);
            prefs->setVrrHistorySeconds(30);
            prefs->setVrrToleranceUs(1500);
        }
        prefs->save();
        const auto savedOptions = prefs->vrrTimingOptions();
        QSettings saved;
        QMap<QString, QVariant> before;
        for (const auto& key : saved.allKeys()) before.insert(key, saved.value(key));
        prefs->reload();
        QSignalSpy modeChanged(prefs, &StreamingPreferences::vrrLatencyModeChanged);
        QSignalSpy timingChanged(prefs, &StreamingPreferences::vrrTimingChanged);

        // The CLI calls this helper only when --vrr-timing-preset is supplied.
        prefs->applyVrrPreset(mode);
        const auto expected = VrrTimingOptions::preset(mode);
        const auto effective = prefs->vrrTimingOptions();
        QCOMPARE(prefs->vrrLatencyMode, mode);
        QCOMPARE(effective.bufferPerMille, expected.bufferPerMille);
        QCOMPARE(effective.targetHundredths, expected.targetHundredths);
        QCOMPARE(effective.historySeconds, expected.historySeconds);
        QCOMPARE(effective.toleranceUs, expected.toleranceUs);
        QCOMPARE(prefs->property("vrrLatencyMode").toInt(), mode);
        QCOMPARE(prefs->property("vrrBufferPerMille").toInt(), expected.bufferPerMille);
        QCOMPARE(prefs->property("vrrTargetHundredths").toInt(), expected.targetHundredths);
        QCOMPARE(prefs->property("vrrHistorySeconds").toInt(), expected.historySeconds);
        QCOMPARE(prefs->property("vrrToleranceUs").toInt(), expected.toleranceUs);
        QCOMPARE(modeChanged.count(), 1);
        QCOMPARE(timingChanged.count(), 1);
        QCOMPARE(prefs->enableVrr, vrrEnabled);
        saved.sync();
        QCOMPARE(saved.allKeys(), before.keys());
        for (auto it = before.cbegin(); it != before.cend(); ++it) QCOMPARE(saved.value(it.key()), it.value());

        // A later launch without the flag still loads the saved mode and overrides.
        prefs->reload();
        QCOMPARE(prefs->vrrLatencyMode, int(StreamingPreferences::VLM_BALANCED));
        QCOMPARE(prefs->vrrBufferPerMille(), savedOptions.bufferPerMille);
        QCOMPARE(prefs->vrrTargetHundredths(), savedOptions.targetHundredths);
        QCOMPARE(prefs->vrrHistorySeconds(), savedOptions.historySeconds);
        QCOMPARE(prefs->vrrToleranceUs(), savedOptions.toleranceUs);
        QCOMPARE(prefs->enableVrr, vrrEnabled);
    }
    void invalidSavedValues()
    {
        QSettings saved;
        saved.setValue("vrrbufferpermille", -9);
        saved.setValue("vrrtargethundredths", 100000);
        saved.setValue("vrrhistoryseconds", "bad");
        saved.setValue("vrrtoleranceus", 1600);
        auto* prefs = StreamingPreferences::get();
        prefs->reload();
        QCOMPARE(prefs->vrrBufferPerMille(), 250);
        QCOMPARE(prefs->vrrTargetHundredths(), 9999);
        QCOMPARE(prefs->vrrHistorySeconds(), 120);
        QCOMPARE(prefs->vrrToleranceUs(), 1500);
        prefs->setVrrToleranceUs(-1);
        prefs->setVrrTargetHundredths(12);
        prefs->setVrrHistorySeconds(99999);
        prefs->setVrrBufferPerMille(99999);
        QCOMPARE(prefs->vrrToleranceUs(), 250);
        QCOMPARE(prefs->vrrTargetHundredths(), 9000);
        QCOMPARE(prefs->vrrHistorySeconds(), 300);
        QCOMPARE(prefs->vrrBufferPerMille(), 4000);
    }
};
QTEST_GUILESS_MAIN(VrrPreferencesTest)
#include "tst_vrrpreferences.moc"
