// Moonlight Dock additions, 2026. GPL-3.0-or-later; see LICENSE.
#pragma once
#include <windows.h>

using DockQuitHandler = bool (*)();

inline LRESULT CALLBACK dockWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto previous = reinterpret_cast<WNDPROC>(GetPropW(window, L"MoonlightDock.PreviousProc"));
    if (message == WM_CLOSE) {
        // A docked SDL window is a child, so do not rely on SDL's last-top-level-
        // window close behavior. Let the stream loop own teardown, just as it does
        // for the controller quit combo. Suppress repeated close requests.
        auto quit = reinterpret_cast<DockQuitHandler>(GetPropW(window, L"MoonlightDock.QuitHandler"));
        if (!GetPropW(window, L"MoonlightDock.QuitPending") && quit && quit()) {
            SetPropW(window, L"MoonlightDock.QuitPending", reinterpret_cast<HANDLE>(1));
        }
        return 0;
    }
    if (message == WM_NCDESTROY) {
        SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
        RemovePropW(window, L"MoonlightDock.PreviousProc");
        RemovePropW(window, L"MoonlightDock.QuitHandler");
        RemovePropW(window, L"MoonlightDock.QuitPending");
        RemovePropW(window, L"MoonlightDock.StreamWindow");
    }
    return CallWindowProcW(previous, window, message, wParam, lParam);
}

// Called on the SDL window's thread while it is still hidden. The host owns resizing
// and focus after this one-time attach. Failure leaves the window hidden for teardown.
inline bool attachDockWindow(HWND child, HWND parent, DockQuitHandler quit)
{
    if (!quit || !IsWindow(child) || !IsWindow(parent) || child == parent ||
        GetPropW(child, L"MoonlightDock.PreviousProc") ||
        IsChild(child, parent) || IsWindowVisible(child)) return false;
    RECT area;
    if (!GetClientRect(parent, &area) || area.right <= 0 || area.bottom <= 0) return false;
    LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
    style = (style & ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME)) | WS_CHILD;
    SetLastError(0);
    if (!SetWindowLongPtrW(child, GWL_STYLE, style) && GetLastError() != 0) return false;
    SetParent(child, parent);
    if (GetParent(child) != parent) return false;
    if (!SetWindowPos(child, HWND_TOP, 0, 0, area.right, area.bottom,
                      SWP_NOACTIVATE | SWP_FRAMECHANGED)) return false;
    auto previous = GetWindowLongPtrW(child, GWLP_WNDPROC);
    if (!SetPropW(child, L"MoonlightDock.PreviousProc", reinterpret_cast<HANDLE>(previous)) ||
        !SetPropW(child, L"MoonlightDock.QuitHandler", reinterpret_cast<HANDLE>(quit)) ||
        !SetPropW(child, L"MoonlightDock.StreamWindow", reinterpret_cast<HANDLE>(1))) {
        RemovePropW(child, L"MoonlightDock.PreviousProc");
        RemovePropW(child, L"MoonlightDock.QuitHandler");
        RemovePropW(child, L"MoonlightDock.StreamWindow");
        return false;
    }
    SetLastError(0);
    if (!SetWindowLongPtrW(child, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(dockWindowProc)) && GetLastError() != 0) {
        RemovePropW(child, L"MoonlightDock.PreviousProc");
        RemovePropW(child, L"MoonlightDock.QuitHandler");
        RemovePropW(child, L"MoonlightDock.StreamWindow");
        return false;
    }
    return true;
}
