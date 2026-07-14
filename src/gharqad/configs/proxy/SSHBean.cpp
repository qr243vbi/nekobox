



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
    bool SSHBean::TryParseJson(const Configs::Data::Node& obj)
    {
        using namespace Configs::From_Json;
        add_default_fields(this->entity, obj);
        user = obj["user"].toString();
        password = obj["password"].toString();
        privateKey = obj["private_key"].toString();
        privateKeyPath = obj["private_key_path"].toString();
        privateKeyPass = obj["private_key_passphrase"].toString();
        hostKey = obj["host_key"].toStringList();
        hostKeyAlgs = obj["host_key_algorithms"].toStringList();
        clientVersion = obj["client_version"].toString();

        return true;
    }

    bool SSHBean::TryParseYaml(const Configs::Data::Node& obj)
    {
        using namespace Configs::From_Yaml;
        add_default_fields(this->entity, obj);
        user = obj["user"].toString();
        password = obj["password"].toString();
        privateKey = obj["private-key"].toString();
        privateKeyPass = obj["private-key-passphrase"].toString();
        hostKey = obj["host-key"].toStringList();
        hostKeyAlgs = obj["host-key-algorithms"].toStringList();

        return true;
    }


    bool SSHBean::TryParseLink(const QString &link) {
        using namespace From_Link;
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);
        add_default_fields(url, entity);

        user = query.queryItemValue("user");
        password = query.queryItemValue("password");
        privateKey = QByteArray::fromBase64(query.queryItemValue("private_key").toUtf8(), QByteArray::OmitTrailingEquals);
        privateKeyPath = query.queryItemValue("private_key_path");
        privateKeyPass = query.queryItemValue("private_key_passphrase");
        auto hostKeysRaw = query.queryItemValue("host_key");
        for (const auto &item: hostKeysRaw.split("-")) {
            auto b64hostKey = QByteArray::fromBase64(item.toUtf8(), QByteArray::OmitTrailingEquals);
            if (!b64hostKey.isEmpty()) hostKey << QString(b64hostKey);
        }
        auto hostKeyAlgsRaw = query.queryItemValue("host_key_algorithms");
        for (const auto &item: hostKeyAlgsRaw.split("-")) {
            auto b64hostKeyAlg = QByteArray::fromBase64(item.toUtf8(), QByteArray::OmitTrailingEquals);
            if (!b64hostKeyAlg.isEmpty()) hostKeyAlgs << QString(b64hostKeyAlg);
        }
        clientVersion = query.queryItemValue("client_version");

        return true;
    }

}