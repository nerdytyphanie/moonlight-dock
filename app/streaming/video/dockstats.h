#pragma once

#include <QtGlobal>
#include <cstdint>
#include <cstddef>
#ifdef Q_OS_WIN32
#include <windows.h>
#endif

struct DockStatsSnapshot {
    uint32_t version;
    uint32_t sequence;
    uint32_t processId;
    uint32_t videoFormat;
    uint64_t updatedTick;
    double videoMbps;
    double incomingFps;
    double networkDropPercent;
    double pacingDropPercent;
    double renderedFps;
    uint32_t rttMs;
    uint32_t rttVarianceMs;
    uint32_t hdr;
    uint32_t active;
};
static_assert(sizeof(DockStatsSnapshot) == 80, "Dock statistics ABI");
static_assert(offsetof(DockStatsSnapshot, videoMbps) == 24, "Dock statistics alignment");

class DockStatsPublisher {
public:
    explicit DockStatsPublisher(bool enabled) {
#ifdef Q_OS_WIN32
        if (!enabled) return;
        wchar_t name[128];
        const DWORD length = GetEnvironmentVariableW(L"MOONLIGHT_DOCK_STATS_MAPPING", name, 128);
        if (!length || length >= 128) return;
        m_Mapping = OpenFileMappingW(FILE_MAP_WRITE, FALSE, name);
        if (m_Mapping) m_View = static_cast<DockStatsSnapshot*>(MapViewOfFile(m_Mapping, FILE_MAP_WRITE, 0, 0, sizeof(DockStatsSnapshot)));
#else
        Q_UNUSED(enabled);
#endif
    }
    ~DockStatsPublisher() {
#ifdef Q_OS_WIN32
        if (m_View) {
            DockStatsSnapshot inactive{};
            publish(inactive);
            UnmapViewOfFile(m_View);
        }
        if (m_Mapping) CloseHandle(m_Mapping);
#endif
    }
    DockStatsPublisher(const DockStatsPublisher&) = delete;
    DockStatsPublisher& operator=(const DockStatsPublisher&) = delete;

    void publish(DockStatsSnapshot snapshot) {
#ifdef Q_OS_WIN32
        if (!m_View) return;
        snapshot.version = 1;
        snapshot.processId = GetCurrentProcessId();
        snapshot.updatedTick = GetTickCount64();
        auto sequence = reinterpret_cast<volatile LONG*>(&m_View->sequence);
        InterlockedIncrement(sequence);
        m_View->version = snapshot.version;
        // Keep the sequence odd throughout the copy; the reader retries torn samples.
        memcpy(&m_View->processId, &snapshot.processId, sizeof(snapshot) - offsetof(DockStatsSnapshot, processId));
        InterlockedIncrement(sequence);
#else
        Q_UNUSED(snapshot);
#endif
    }
private:
#ifdef Q_OS_WIN32
    HANDLE m_Mapping = nullptr;
    DockStatsSnapshot* m_View = nullptr;
#endif
};
