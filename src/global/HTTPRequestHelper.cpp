#include <QCoreApplication>
#ifdef Q_OS_WIN
    #include <windows.h>
#endif
#undef boolean

#include "include/global/HTTPRequestHelper.hpp"

#include <QNetworkProxy>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QFile>
#include <QApplication>

#include "include/global/Configs.hpp"
#include "include/ui/mainwindow.h"

namespace Configs_network {

    namespace {
        struct DeviceDetails {
            QString hwid;
            QString os;
            QString osVersion;
            QString model;
        };

#ifdef Q_OS_WIN
    static QString readRegistryStringValue(const QString &keyPath, const QString &valueName) {
        HKEY hKey = nullptr;
        std::wstring keyPathW = keyPath.toStdWString();
        LONG res = RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPathW.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
        if (res != ERROR_SUCCESS) return QString();
        DWORD type = 0;
        DWORD cbData = 0;
        res = RegQueryValueExW(hKey, (LPCWSTR)valueName.utf16(), nullptr, &type, nullptr, &cbData);
        if (res != ERROR_SUCCESS || type != REG_SZ) {
            RegCloseKey(hKey);
            return QString();
        }
        std::vector<wchar_t> buffer(cbData/sizeof(wchar_t) + 1);
        res = RegQueryValueExW(hKey, (LPCWSTR)valueName.utf16(), nullptr, nullptr, reinterpret_cast<LPBYTE>(buffer.data()), &cbData);
        if (res != ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return QString();
        }
        RegCloseKey(hKey);
        return QString::fromWCharArray(buffer.data());
    }
#endif

    static DeviceDetails GetDeviceDetails() {
        DeviceDetails details;

#ifdef Q_OS_WIN
        const QString regPath = QString::fromUtf8("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SoftwareProtectionPlatform\\Plugins\\Objects\\msft:rm/algorithm/hwid/4.0");
        const QString valueName = QString::fromUtf8("ModuleId");
        details.hwid = readRegistryStringValue(regPath, valueName);

        if (details.hwid.isEmpty()) {
            auto productId = QSysInfo::machineUniqueId();
            if (productId.isEmpty()) productId = QSysInfo::productType().toUtf8();
            details.hwid = QString("%1-%2").arg(QSysInfo::machineHostName(), QString::fromUtf8(productId));
        }

        details.os = QStringLiteral("Windows");

        VersionInfo info;
        WinVersion::GetVersion(info);
        details.osVersion = QString("%1.%2.%3").arg(info.Major).arg(info.Minor).arg(info.BuildNum);

        details.model = QSysInfo::prettyProductName();
#elif defined(Q_OS_LINUX)
        QString mid;
        QFile f1("/etc/machine-id");
        if (f1.exists() && f1.open(QIODevice::ReadOnly | QIODevice::Text)) {
            mid = QString::fromUtf8(f1.readAll()).trimmed();
            f1.close();
        } else {
            QFile f2("/var/lib/dbus/machine-id");
            if (f2.exists() && f2.open(QIODevice::ReadOnly | QIODevice::Text)) {
                mid = QString::fromUtf8(f2.readAll()).trimmed();
                f2.close();
            }
        }
        details.hwid = mid;
        details.os = QStringLiteral("Linux");
        details.osVersion = QSysInfo::kernelVersion();
        details.model = QSysInfo::prettyProductName();
#elif defined(Q_OS_MACOS)
        details.hwid = QSysInfo::machineUniqueId();
        details.os = QStringLiteral("macOS");
        details.osVersion = QSysInfo::productVersion();
        details.model = QSysInfo::prettyProductName();
#else
        details.hwid = QSysInfo::machineUniqueId();
        details.os = QSysInfo::productType();
        details.osVersion = QSysInfo::productVersion();
        details.model = QSysInfo::prettyProductName();
#endif
        return details;
    }
} 

    HTTPResponse NetworkRequestHelper::HttpGet(const QString &url) {
        QNetworkRequest request;
        QNetworkAccessManager accessManager;
        request.setUrl(url);
        if (Configs::dataStore->sub_use_proxy || Configs::dataStore->spmode_system_proxy) {
            if (Configs::dataStore->started_id < 0) {
                return HTTPResponse{QObject::tr("Request with proxy but no profile started.")};
            }
            QNetworkProxy p;
            p.setType(QNetworkProxy::HttpProxy);
            p.setHostName("127.0.0.1");
            p.setPort(Configs::dataStore->inbound_socks_port);
            accessManager.setProxy(p);
        }
        // Set attribute
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, Configs::dataStore->GetUserAgent());
        if (Configs::dataStore->sub_insecure) {
            QSslConfiguration c;
            c.setPeerVerifyMode(QSslSocket::PeerVerifyMode::VerifyNone);
            request.setSslConfiguration(c);
        }
        //Attach HWID and device info headers if enabled in settings
        if (Configs::dataStore->sub_send_hwid && !request.url().toString().contains("/throneproj/")) {
            auto details = GetDeviceDetails();
           
            if (!details.hwid.isEmpty()) request.setRawHeader("x-hwid", details.hwid.toUtf8());
            if (!details.os.isEmpty()) request.setRawHeader("x-device-os", details.os.toUtf8());
            if (!details.osVersion.isEmpty()) request.setRawHeader("x-ver-os", details.osVersion.toUtf8());
            if (!details.model.isEmpty()) request.setRawHeader("x-device-model", details.model.toUtf8());
        }
        //
        auto _reply = accessManager.get(request);
        connect(_reply, &QNetworkReply::sslErrors, _reply, [](const QList<QSslError> &errors) {
            QStringList error_str;
            for (const auto &err: errors) {
                error_str << err.errorString();
            }
            MW_show_log(QString("SSL Errors: %1 %2").arg(error_str.join(","), Configs::dataStore->sub_insecure ? "(Ignored)" : ""));
        });
        // Wait for response
        auto abortTimer = new QTimer;
        abortTimer->setSingleShot(true);
        abortTimer->setInterval(10000);
        connect(abortTimer, &QTimer::timeout, _reply, &QNetworkReply::abort);
        abortTimer->start();
        {
            QEventLoop loop;
            connect(_reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();
        }
        if (abortTimer != nullptr) {
            abortTimer->stop();
            abortTimer->deleteLater();
        }
        //
        auto result = HTTPResponse{_reply->error() == QNetworkReply::NetworkError::NoError ? "" : _reply->errorString(),
                                       _reply->readAll(), _reply->rawHeaderPairs()};
        _reply->deleteLater();
        return result;
    }

    QString NetworkRequestHelper::GetHeader(const QList<QPair<QByteArray, QByteArray>> &header, const QString &name) {
        for (const auto &p: header) {
            if (QString(p.first).toLower() == name.toLower()) return p.second;
        }
        return "";
    }

    QString NetworkRequestHelper::DownloadAsset(const QString &url, const QString &fileName) {
        QNetworkRequest request;
        QNetworkAccessManager accessManager;
        request.setUrl(url);
        if (Configs::dataStore->spmode_system_proxy) {
            if (Configs::dataStore->started_id < 0) {
                return QObject::tr("Request with proxy but no profile started.");
            }
            QNetworkProxy p;
            p.setType(QNetworkProxy::HttpProxy);
            p.setHostName("127.0.0.1");
            p.setPort(Configs::dataStore->inbound_socks_port);
            accessManager.setProxy(p);
        }
        request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, Configs::dataStore->GetUserAgent());
        request.setRawHeader("Accept", "application/octet-stream");

        auto _reply = accessManager.get(request);
        connect(_reply, &QNetworkReply::sslErrors, _reply, [](const QList<QSslError> &errors) {
            QStringList error_str;
            for (const auto &err: errors) {
                error_str << err.errorString();
            }
            MW_show_log(QString("SSL Errors: %1").arg(error_str.join(",")));
        });
        connect(_reply, &QNetworkReply::downloadProgress, _reply, [&](qint64 bytesReceived, qint64 bytesTotal)
        {
            runOnUiThread([=]{
                GetMainWindow()->setDownloadReport(DownloadProgressReport{fileName, bytesReceived, bytesTotal}, true);
                GetMainWindow()->UpdateDataView();
            });
        });
        QEventLoop loop;
        connect(_reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        runOnUiThread([=]
        {
            GetMainWindow()->setDownloadReport({}, false);
            GetMainWindow()->UpdateDataView(true);
        });
        if(_reply->error() != QNetworkReply::NetworkError::NoError) {
            return _reply->errorString();
        }

        auto filePath = Configs::GetBasePath()+ "/" + fileName;
        auto file = QFile(filePath);
        if (file.exists()) {
            file.remove();
        }
        if (!file.open(QIODevice::WriteOnly)) {
            return QObject::tr("Could not open file.");
        }
        file.write(_reply->readAll());
        file.close();
        _reply->deleteLater();
        return "";
    }

} // namespace Configs_network
