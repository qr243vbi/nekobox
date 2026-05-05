#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {
    

    CoreObjOutboundBuildResult SSHBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;
        using namespace To_CoreObj_box;
        QJsonObject outbound{
            {"type", "ssh"},
            {"server", entity->serverAddress},
            {"server_port", entity->serverPort},
            {"user", user},
            {"password", password},
        };
        add_non_empty(outbound, "private_key", privateKey);
        add_non_empty(outbound, "private_key_path", privateKeyPath);
        add_non_empty(outbound, "private_key_passphrase", privateKeyPass);
        if (!hostKey.isEmpty()) outbound["host_key"] = QListStr2QJsonArray(hostKey);
        if (!hostKeyAlgs.isEmpty()) outbound["host_key_algorithms"] = QListStr2QJsonArray(hostKeyAlgs);
        add_non_empty(outbound, "client_version", clientVersion);

        result.outbound = outbound;
        return result;
    }

    QString SSHBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        url.setScheme("ssh");
        add_default_fields(url, this);
        QUrlQuery q;
        add_query_nonempty("user", q, user);
        add_query_nonempty("password", q, password);
        add_query_nonempty("private_key", q, privateKey.toUtf8().toBase64(QByteArray::OmitTrailingEquals));
        add_query_nonempty("private_key_path", q, privateKeyPath);
        add_query_nonempty("private_key_passphrase", q, privateKeyPass);
        QStringList b64HostKeys = {};
        for (const auto& item: hostKey) {
            b64HostKeys << item.toUtf8().toBase64(QByteArray::OmitTrailingEquals);
        }
        add_query_nonempty("host_key", q, b64HostKeys.join("-"));
        QStringList b64HostKeyAlgs = {};
        for (const auto& item: hostKeyAlgs) {
            b64HostKeyAlgs << item.toUtf8().toBase64(QByteArray::OmitTrailingEquals);
        }
        add_query_nonempty("host_key_algorithms", q, b64HostKeyAlgs.join("-"));
        add_query_nonempty("client_version", q, clientVersion);
        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }

}