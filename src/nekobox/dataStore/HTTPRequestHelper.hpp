#pragma once
#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ConfigItem.hpp>
#include <QObject>
#include <QString>
#include <QMap>
#include <cpr/proxyauth.h>
#include <cpr/cpr.h>

namespace Configs_network {
    enum HTTPMethod {
        Get = 0,
        Post = 1,
        Head = 2
    };


    void BuildSession(const QString &url, bool sendHwid, cpr::Session& session, QMap<QString, QString> headerMap = {}, QByteArray payload = "");

    struct HTTPResponse {
        QString error;
        QByteArray data;
        QMap<EnumFieldName, QString> header;
    };

    struct DownloadProgressReport
    {
        QString fileName;
        qint64 downloadedSize;
        qint64 totalSize;
    };
    class NetworkRequestHelper : QObject {
        Q_OBJECT

        explicit NetworkRequestHelper(QObject *parent) : QObject(parent){};

        ~NetworkRequestHelper() override = default;
        ;

    public:
        template<HTTPMethod method>
        static HTTPResponse HttpJob(const QString &url, bool sendHwid = false, QMap<QString, QString> headers = {}, QByteArray payload = "");

        static HTTPResponse HttpGet(const QString &url, bool sendHwid = false, QMap<QString, QString> headers = {}, QByteArray payload = "");
        static HTTPResponse HttpPost(const QString &url, bool sendHwid = false, QMap<QString, QString> headers = {}, QByteArray payload = "");
        static HTTPResponse HttpHead(const QString &url, bool sendHwid = false, QMap<QString, QString> headers = {}, QByteArray payload = "");

        static QString GetHeader(const QMap<EnumFieldName, QString> &header, const QString &name);
    };

}

using namespace Configs_network;
