// Moonlight Dock additions, 2026. GPL-3.0-or-later; see LICENSE.
#pragma once
#include <QCoreApplication>
#include <QVariant>

// Inspect arguments even before parsing, so invalid dock launches never open a message box.
inline bool dockRequested()
{
    for (const QString& arg : QCoreApplication::arguments()) {
        if (arg == "--dock-parent" || arg.startsWith("--dock-parent=")) return true;
    }
    return false;
}

inline quintptr dockParentHandle()
{
    return static_cast<quintptr>(QCoreApplication::instance()->property("dockParent").toULongLong());
}
