#include "include/global/HTTPRequestHelper.hpp"

#include <QNetworkProxy>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QFile>
#include <QApplication>
#include <QFileInfo>
#include <QMap>
#include <QStringList>
#include "include/dataStore/Configs.hpp"
#include "include/ui/mainwindow.h"
#include "include/global/DeviceDetailsHelper.hpp"

static inline void InitializeRequest(
    QNetworkRequest & request, 
    QNetworkAccessManager & accessManager,
    const QString & url,
    QString & error,
    bool sendHwid
){
        bool net_use_proxy = !Configs::dataStore->net_skip_proxy;
        bool proxy_started = Configs::dataStore->started_id >= 0;
        net_use_proxy &= proxy_started;
        net_use_proxy |= Configs::dataStore->spmode_system_proxy;
        
        accessManager.setTransferTimeout(10000);
        request.setUrl(url);
        if (net_use_proxy) {
            if (!proxy_started) {
                error = QObject::tr("Request with proxy but no profile started.");
                return;
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
        if (Configs::dataStore->net_insecure) {
            QSslConfiguration c;
            c.setPeerVerifyMode(QSslSocket::PeerVerifyMode::VerifyNone);
            request.setSslConfiguration(c);
        }
        //Attach HWID and device info headers if enabled in settings
        if (sendHwid) {
            auto details = GetDeviceDetails();
           
            
            // Parse custom parameters if provided
            QMap<QString, QString> customParams;
            if (!Configs::dataStore->sub_custom_hwid_params.isEmpty()) {
                QStringList pairs = Configs::dataStore->sub_custom_hwid_params.split(',');
                for (const QString &pair : pairs) {
                    QString trimmed = pair.trimmed();
                    int eqPos = trimmed.indexOf('=');
                    if (eqPos > 0) {
                        QString key = trimmed.left(eqPos).trimmed();
                        QString value = trimmed.mid(eqPos + 1).trimmed();
                        // Validate: key must be one of the allowed parameters, value must not contain newlines
                        if (!key.isEmpty() && !value.isEmpty() &&
                            !value.contains('\n') && !value.contains('\r') &&
                            value.length() < 1000) { // Reasonable length limit
                            QString lowerKey = key.toLower();
                            // Only accept known parameter keys
                            if (lowerKey == "hwid" || lowerKey == "os" ||
                                lowerKey == "osversion" || lowerKey == "model") {
                                customParams[lowerKey] = value;
                            }
                        }
                    }
                }
            }

            // Use custom values if provided, otherwise use default values
            QString hwid = customParams.contains("hwid") ? customParams["hwid"] : details.hwid;
            QString os = customParams.contains("os") ? customParams["os"] : details.os;
            QString osVersion = customParams.contains("osversion") ? customParams["osversion"] : details.osVersion;
            QString model = customParams.contains("model") ? customParams["model"] : details.model;

            if (!hwid.isEmpty()) request.setRawHeader("x-hwid", hwid.toUtf8());
            if (!os.isEmpty()) request.setRawHeader("x-device-os", os.toUtf8());
            if (!osVersion.isEmpty()) request.setRawHeader("x-ver-os", osVersion.toUtf8());
            if (!model.isEmpty()) request.setRawHeader("x-device-model", model.toUtf8());
        }
}

namespace Configs_network {

    HTTPResponse NetworkRequestHelper::HttpGet(const QString &url, bool sendHwid) {
        QNetworkRequest request;
        QNetworkAccessManager accessManager;
        QString error;

        InitializeRequest(request, accessManager, url, error, sendHwid);

        if (!error.isEmpty()){
            return HTTPResponse{error};
        }
        //
        auto _reply = accessManager.get(request);
        connect(_reply, &QNetworkReply::sslErrors, _reply, [](const QList<QSslError> &errors) {
            QStringList error_str;
            for (const auto &err: errors) {
                error_str << err.errorString();
            }
            MW_show_log(QString("SSL Errors: %1 %2").arg(error_str.join(","), Configs::dataStore->net_insecure ? "(Ignored)" : ""));
        });
        // Wait for response
        QEventLoop loop;
        connect(_reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        
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
        QString error;

        InitializeRequest(request, accessManager, url, error, false);
        if (!error.isEmpty()){
            return error;
        }

        auto _reply = accessManager.get(request);
        connect(_reply, &QNetworkReply::sslErrors, _reply, [](const QList<QSslError> &errors) {
            QStringList error_str;
            for (const auto &err: errors) {
                error_str << err.errorString();
            }
            MW_show_log(QString("SSL Errors: %1 %2").arg(error_str.join(","), Configs::dataStore->net_insecure ? "(Ignored)" : ""));
        });
        bool show_rule_set;
        if (!(show_rule_set = GetMainWindow()->isShowRuleSetData())){
        connect(_reply, &QNetworkReply::downloadProgress, _reply, [&](qint64 bytesReceived, qint64 bytesTotal)
        {
            runOnUiThread([=]{
                GetMainWindow()->setDownloadReport(DownloadProgressReport{fileName, bytesReceived, bytesTotal}, true);
                GetMainWindow()->UpdateDataView();
            });
        });
        }
        QEventLoop loop;
        connect(_reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        if(!show_rule_set){
        runOnUiThread([=]
        {
            GetMainWindow()->setDownloadReport({}, false);
            GetMainWindow()->UpdateDataView(true);
        });
        }
        _reply->deleteLater();
        if(_reply->error() != QNetworkReply::NetworkError::NoError) {
            return _reply->errorString();
        }

        auto filePath = Configs::GetBasePath()+ "/" + fileName;
        auto file = QFile(filePath);
        if (file.exists()) {
            file.remove();
        } else {
            QFileInfo info(file);
            QString path = info.absolutePath();
            QDir dir(path);
            if (!dir.exists()){
                dir.mkpath(path);
            }
        }
        if (!file.open(QIODevice::WriteOnly)) {
            return QObject::tr("Could not open file.");
        }
        file.write(_reply->readAll());
        file.close();
        return "";
    }

} // namespace Configs_network
