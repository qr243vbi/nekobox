#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once
#include <QHostAddress>
#include <QObject>
#include <QString>
//
namespace Qv2ray::components::proxy {
    void ClearSystemProxy();
    void SetSystemProxy(int http_port, int socks_port, QString scheme);
} // namespace Qv2ray::components::proxy

using namespace Qv2ray::components;
using namespace Qv2ray::components::proxy;
