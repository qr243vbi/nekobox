#include "HTTPRequestHelper.hpp"

#include <QByteArray>
#include <QMetaEnum>
#include <QTimer>
#include "cpr/cpr.h"

#include "main/NekoGui.hpp"

namespace NekoGui_network {

    NekoHTTPResponse NetworkRequestHelper::HttpGet(const QString &url) {
        cpr::Session session;
        if (NekoGui::dataStore->sub_use_proxy) {
            session.SetProxies({{"http", "127.0.0.1:" + QString(Int2String(NekoGui::dataStore->inbound_socks_port)).toStdString()},
                                {"https", "127.0.0.1:" + QString(Int2String(NekoGui::dataStore->inbound_socks_port)).toStdString()}});
            if (NekoGui::dataStore->inbound_auth->NeedAuth()) {
                session.SetProxyAuth(cpr::ProxyAuthentication{{"http",
                cpr::EncodedAuthentication{NekoGui::dataStore->inbound_auth->username.toStdString(), NekoGui::dataStore->inbound_auth->password.toStdString()}}});
            }
            if (NekoGui::dataStore->started_id < 0) {
                return NekoHTTPResponse{QObject::tr("Request with proxy but no profile started.")};
            }
        }
        if (NekoGui::dataStore->sub_insecure) {
            session.SetVerifySsl(cpr::VerifySsl{false});
        }
        session.SetUserAgent(cpr::UserAgent{NekoGui::dataStore->GetUserAgent().toStdString()});
        session.SetTimeout(cpr::Timeout(10000));
        session.SetUrl(cpr::Url(url.toStdString()));
        auto resp = session.Get();
        auto headerPairs = QList<QPair<QByteArray, QByteArray>>();
        for (const auto &item: resp.header) {
            headerPairs.append(std::pair<QByteArray, QByteArray>(QByteArray(item.first.c_str()), QByteArray(item.second.c_str())));
        }
        auto err = resp.error.message.empty() ? (resp.status_code == 200 ? "" : resp.status_line) : resp.error.message;
        auto result = NekoHTTPResponse{ err.c_str(),
                                       resp.text.c_str(), headerPairs};
        return result;
    }

    QString NetworkRequestHelper::GetHeader(const QList<QPair<QByteArray, QByteArray>> &header, const QString &name) {
        for (const auto &p: header) {
            if (QString(p.first).toLower() == name.toLower()) return p.second;
        }
        return "";
    }

} // namespace NekoGui_network
