// Moonlight Dock additions, 2026. GPL-3.0-or-later; see LICENSE.
// Headless startup check: never starts a real stream (uses the documentation-only TEST-NET address).
#include <windows.h>
#include <string>
#include <cstdio>
static DWORD childPid;
static unsigned shows;
static void CALLBACK onShow(HWINEVENTHOOK, DWORD, HWND window, LONG object, LONG, DWORD, DWORD)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid == childPid && object == OBJID_WINDOW) ++shows;
}
int wmain(int argc, wchar_t** argv)
{
    if (argc != 2) return 2;
    HWND host = CreateWindowExW(0, L"STATIC", L"Hidden embedding test", WS_OVERLAPPEDWINDOW,
                                0, 0, 800, 600, nullptr, nullptr, nullptr, nullptr);
    wchar_t handle[40];
    swprintf_s(handle, L"0x%llx", static_cast<unsigned long long>(reinterpret_cast<UINT_PTR>(host)));
    std::wstring command = L"\"" + std::wstring(argv[1]) + L"\" stream --dock-parent " + handle + L" 192.0.2.1 Desktop";
    STARTUPINFOW startup = { sizeof(startup) };
    PROCESS_INFORMATION process = {};
    auto hook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr, onShow, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!host || !hook || !CreateProcessW(nullptr, &command[0], nullptr, nullptr, FALSE,
                                         CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) return 3;
    childPid = process.dwProcessId;
    const ULONGLONG deadline = GetTickCount64() + 45000;
    while (WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT && GetTickCount64() < deadline) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
    DWORD exitCode;
    GetExitCodeProcess(process.hProcess, &exitCode);
    if (exitCode == STILL_ACTIVE) { TerminateProcess(process.hProcess, 99); WaitForSingleObject(process.hProcess, 5000); }
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    UnhookWinEvent(hook);
    DestroyWindow(host);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    std::printf("Headless failed-host launch: exit=%lu, visible-window events=%u\n", exitCode, shows);
    return exitCode != 0 && exitCode != STILL_ACTIVE && shows == 0 ? 0 : 1;
}
