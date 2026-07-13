#pragma once
#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ConfigItem.hpp>
#include <QObject>
#include <QString>
#include <QMap>

namespace cpr {
    class Session {

    };
}

namespace Configs_network {
    enum HTTPMethod {
        Get = 0,
        Post = 1,
        Head = 2
    };

    QMap<QString, QString> GetHWID(const QString & sub_custom_hwid_params);

    void BuildSession(const QString &url, bool sendHwid, cpr::Session& session, const QMap<QString, QString> &headerMap = {}, const QByteArray &payload = "");

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
        static HTTPResponse HttpJob(const QString &url, bool sendHwid = false, const QMap<QString, QString>& headers = {}, const QByteArray& payload = "");

        static HTTPResponse HttpGet(const QString &url, bool sendHwid = false, const QMap<QString, QString>& headers = {}, const QByteArray& payload = "");
        static HTTPResponse HttpPost(const QString &url, bool sendHwid = false, const QMap<QString, QString>& headers = {}, const QByteArray& payload = "");
        static HTTPResponse HttpHead(const QString &url, bool sendHwid = false, const QMap<QString, QString>& headers = {}, const QByteArray& payload = "");

        static QString GetHeader(const QMap<EnumFieldName, QString> &header, const QString &name);
    };

}

using namespace Configs_network;
