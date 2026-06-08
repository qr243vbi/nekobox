
#include <nekobox/dataStore/HTTPRequestHelper.hpp>

#include <QString>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QMap>

#include <string>

#include <nekobox/dataStore/Configs.hpp>
#include <nekobox/global/DeviceDetailsHelper.hpp>
#include <nekobox/ui/mainwindow.h>

void Configs_network::BuildSession(const QString &url, bool sendHwid, cpr::Session& session,
                                   const QMap<QString, QString> &headerMap, const QByteArray& payload)
{
    auto s = Configs::dataStore;

    session.SetUrl(cpr::Url{url.toStdString()});
    session.SetTimeout(cpr::Timeout{Configs::dataStore->download_timeout});

    // ------------------------
    // USER AGENT
    // ------------------------
    session.SetHeader({
        {"User-Agent", s->GetUserAgent().toStdString()}
    });

    // ------------------------
    // SSL
    // ------------------------
    if (s->net_insecure) {
        session.SetVerifySsl(cpr::VerifySsl{false});
    }

    // ------------------------
    // PROXY
    // ------------------------
    bool useProxy = s->useProxyForHttpRequest() || s->spmode_system_proxy;
    bool proxyStarted = s->started_id >= 0;

    if (useProxy && proxyStarted)
    {
        QString host = (s->inbound_address == "::")
            ? "127.0.0.1"
            : s->inbound_address;

        std::string proxy =
            host.toStdString() + ":" +
            std::to_string(s->inbound_socks_port);

        session.SetProxies({
            {"http", proxy},
            {"https", proxy},
        });

        if (!s->inbound_username.isEmpty() &&
            !s->inbound_password.isEmpty())
        {
            auto user = s->inbound_username.toStdString();
            auto pass = s->inbound_password.toStdString();
            session.SetProxyAuth({
                {"http", cpr::EncodedAuthentication{user, pass}},   
                {"https", cpr::EncodedAuthentication{user, pass}}
            });
        }
    }

    // ------------------------
    // HWID HEADERS
    // ------------------------
    cpr::Header headers;
    if (sendHwid){        
        for (auto [key, value]: asKeyValueRange(GetHWID(s->sub_custom_hwid_params))){
            headers[key.toStdString()] = value.toStdString();
        }
    }

    for (auto [key, value]: asKeyValueRange(headerMap)){
        headers[key.toStdString()] = value.toStdString();
    }

    session.SetBody(cpr::Body{payload.toStdString()});

    session.UpdateHeader(headers);
}


QMap<QString, QString> Configs_network::GetHWID(const QString & sub_custom_hwid_params){
        auto d = GetDeviceDetails();
        QMap<QString, QString> headers;

        QMap<QString, QString> custom;

        if (!sub_custom_hwid_params.isEmpty()) {
            const QStringList pairs = sub_custom_hwid_params.split(',');

            for (const auto& p : pairs) {
                const auto t = p.trimmed();
                const int eq = t.indexOf('=');

                if (eq > 0) {
                    custom[t.left(eq).toLower()] =
                        t.mid(eq + 1).trimmed();
                }
            }
        }

        const auto hwid  = custom.value("hwid", d.hwid);
        const auto os    = custom.value("os", d.os);
        const auto osv   = custom.value("osversion", d.osVersion);
        const auto model = custom.value("model", d.model);

        if (!hwid.isEmpty())
            headers.insert("x-hwid", hwid);

        if (!os.isEmpty())
            headers.insert("x-device-os", os);

        if (!osv.isEmpty())
            headers.insert("x-ver-os", osv);

        if (!model.isEmpty())
            headers.insert("x-device-model", model);
        return headers;
}

namespace Configs_network
{
template<HTTPMethod method>
HTTPResponse NetworkRequestHelper::HttpJob(const QString &url, bool sendHwid, const QMap<QString, QString>& headers, const QByteArray& payload )
{
    cpr::Session session; 
    BuildSession(url, sendHwid, session, headers, payload);
    cpr::Response r ;

    if constexpr (method == HTTPMethod::Get){
        r = session.Get();
    } else if constexpr (method == HTTPMethod::Post){
        r = session.Post();
    } else if constexpr (method == HTTPMethod::Head){
        r = session.Head();
    }

    auto resp = HTTPResponse{
        r.error ? QString::fromStdString(r.error.message) : "",
        QByteArray(r.text.data(), r.text.size()),
        {}
    };

    resp.header = QStdMapString2QMapEnumFieldName(r.header);

    return resp;
}

QString NetworkRequestHelper::GetHeader(const QMap<EnumFieldName, QString> &header, const QString &name) {
    return header.value(name, "");
}


template HTTPResponse NetworkRequestHelper::HttpJob<HTTPMethod::Get>(const QString&, bool, const QMap<QString, QString>&,const QByteArray &payload);
template HTTPResponse NetworkRequestHelper::HttpJob<HTTPMethod::Post>(const QString&, bool, const QMap<QString, QString>&,const QByteArray &payload);
template HTTPResponse NetworkRequestHelper::HttpJob<HTTPMethod::Head>(const QString&, bool, const QMap<QString, QString>&,const QByteArray &payload);

HTTPResponse NetworkRequestHelper::HttpGet(const QString &url, bool sendHwid, const QMap<QString, QString>& headers, const QByteArray& payload ){
    return NetworkRequestHelper::HttpJob<HTTPMethod::Get>(url, sendHwid, headers, payload);
};
HTTPResponse NetworkRequestHelper::HttpPost(const QString &url, bool sendHwid, const QMap<QString, QString>& headers, const QByteArray& payload){
    return NetworkRequestHelper::HttpJob<HTTPMethod::Post>(url, sendHwid, headers, payload);
};
HTTPResponse NetworkRequestHelper::HttpHead(const QString &url, bool sendHwid, const QMap<QString, QString>& headers, const QByteArray& payload){
    return NetworkRequestHelper::HttpJob<HTTPMethod::Head>(url, sendHwid, headers, payload);
};

}
