// Moonlight Dock additions, 2026. GPL-3.0-or-later; see LICENSE.
#include "../app/streaming/dockwindow.h"
#include <cassert>
#include <cstdio>

static int quitRequests;
static bool acceptQuit;
static bool requestQuit()
{
    ++quitRequests;
    return acceptQuit;
}

int main()
{
    HWND parent = CreateWindowExW(0, L"STATIC", L"Dock test host", WS_OVERLAPPEDWINDOW,
                                  0, 0, 640, 480, nullptr, nullptr, nullptr, nullptr);
    HWND child = CreateWindowExW(0, L"STATIC", L"Dock test stream", WS_POPUP | WS_CAPTION,
                                 0, 0, 100, 100, nullptr, nullptr, nullptr, nullptr);
    assert(parent && child);
    assert(!IsWindowVisible(child));
    assert(!attachDockWindow(child, nullptr, requestQuit));
    assert(!attachDockWindow(child, child, requestQuit));
    assert(!attachDockWindow(child, parent, nullptr));
    assert(attachDockWindow(child, parent, requestQuit));
    assert(!attachDockWindow(child, parent, requestQuit));
    assert(GetParent(child) == parent);
    assert(!IsWindowVisible(child));
    assert((GetWindowLongPtrW(child, GWL_STYLE) & WS_CHILD) != 0);
    assert((GetWindowLongPtrW(child, GWL_STYLE) & (WS_POPUP | WS_CAPTION)) == 0);
    assert(GetPropW(child, L"MoonlightDock.StreamWindow") == reinterpret_cast<HANDLE>(1));
    RECT parentRect, childRect;
    GetClientRect(parent, &parentRect);
    GetClientRect(child, &childRect);
    assert(parentRect.right == childRect.right && parentRect.bottom == childRect.bottom);
    // The host posts WM_CLOSE. It must reach the quit handler without destroying
    // the child, and a rejected queue attempt must permit the next request.
    assert(PostMessageW(child, WM_CLOSE, 0, 0));
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&message);
    assert(quitRequests == 1 && IsWindow(child));
    acceptQuit = true;
    SendMessageW(child, WM_CLOSE, 0, 0);
    assert(quitRequests == 2 && IsWindow(child));
    SendMessageW(child, WM_CLOSE, 0, 0);
    assert(quitRequests == 2 && IsWindow(child));
    // Unrelated window messages still reach the original window procedure.
    SendMessageW(child, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L"Still alive"));
    wchar_t title[32];
    assert(GetWindowTextW(child, title, 32) == 11);
    DestroyWindow(parent);
    assert(!IsWindow(child));
    std::puts("Dock window checks passed: attach, graceful close, retry, repeated close, message forwarding, teardown.");
}
