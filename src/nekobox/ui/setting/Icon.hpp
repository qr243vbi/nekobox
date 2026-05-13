#ifdef _WIN32
#include <winsock2.h>
#endif

#pragma once

#include <QPixmap>

namespace Icon {

    enum TrayIconStatus {
        NONE,
        RUNNING,
        SYSTEM_PROXY,
        VPN,
        DNS,
        SYSTEM_PROXY_DNS,
    };

    QPixmap GetTrayIcon(TrayIconStatus status = TrayIconStatus::NONE);
} // namespace Icon
