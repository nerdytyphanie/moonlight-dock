// Moonlight Dock additions, 2026. GPL-3.0-or-later; see LICENSE.
#include "../app/streaming/dockwindow.h"
#include <cassert>
#include <cstdio>

int main()
{
    HWND parent = CreateWindowExW(0, L"STATIC", L"Dock test host", WS_OVERLAPPEDWINDOW,
                                  0, 0, 640, 480, nullptr, nullptr, nullptr, nullptr);
    HWND child = CreateWindowExW(0, L"STATIC", L"Dock test stream", WS_POPUP | WS_CAPTION,
                                 0, 0, 100, 100, nullptr, nullptr, nullptr, nullptr);
    assert(parent && child);
    assert(!IsWindowVisible(child));
    assert(!attachDockWindow(child, nullptr));
    assert(!attachDockWindow(child, child));
    assert(attachDockWindow(child, parent));
    assert(GetParent(child) == parent);
    assert(!IsWindowVisible(child));
    assert((GetWindowLongPtrW(child, GWL_STYLE) & WS_CHILD) != 0);
    assert((GetWindowLongPtrW(child, GWL_STYLE) & (WS_POPUP | WS_CAPTION)) == 0);
    assert(GetPropW(child, L"MoonlightDock.StreamWindow") == reinterpret_cast<HANDLE>(1));
    RECT parentRect, childRect;
    GetClientRect(parent, &parentRect);
    GetClientRect(child, &childRect);
    assert(parentRect.right == childRect.right && parentRect.bottom == childRect.bottom);
    DestroyWindow(parent);
    assert(!IsWindow(child));
    std::puts("Dock window checks passed: hidden attach, styles, size, marker, parent teardown.");
}
