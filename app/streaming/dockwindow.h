// Moonlight Dock additions, 2026. GPL-3.0-or-later; see LICENSE.
#pragma once
#include <windows.h>

// Called on the SDL window's thread while it is still hidden. The host owns resizing
// and focus after this one-time attach. Failure leaves the window hidden for teardown.
inline bool attachDockWindow(HWND child, HWND parent)
{
    if (!IsWindow(child) || !IsWindow(parent) || child == parent ||
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
    return SetPropW(child, L"MoonlightDock.StreamWindow", reinterpret_cast<HANDLE>(1)) != FALSE;
}
