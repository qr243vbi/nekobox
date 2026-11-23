#pragma once

#include <QStringList>

namespace LaunchOptions {
inline bool useAppData(const QStringList &arguments, bool windows) {
    return arguments.contains("-appdata") ||
           (windows && !arguments.contains("-portable"));
}
}
