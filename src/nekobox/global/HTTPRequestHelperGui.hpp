#pragma once
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <nekobox/dataStore/HTTPRequestHelper.hpp>

#include <QObject>
#include <functional>

#include <cpr/proxyauth.h>
#include <cpr/cpr.h>

namespace Configs_network {


    namespace NetworkRequestHelperGui {
        QString DownloadAsset(const QString &url, const QString &fileName);
    }

} // namespace Configs_network

using namespace Configs_network;
