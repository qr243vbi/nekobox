#include <QString>
#include <QJsonDocument>
#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <nekobox/configs/proxy/WireguardBean.h>
#include <3rdparty/ini/ini.h>

namespace Configs {

    CoreObjOutboundBuildResult WireguardBean::BuildCoreObjSingBox() const {
        if (is_amnezia){
            return this->BuildCoreObjSingBoxAwg();
        }
        CoreObjOutboundBuildResult result;
        using namespace To_CoreObj_box;
        auto tun_name = "wg_" + GetRandomString(9, ExcludeUppercase | ExcludeDigits);

        QJsonObject peer{
            {"address", entity->serverAddress},
            {"port", entity->serverPort},
            {"public_key", publicKey},
            {"pre_shared_key", preSharedKey},
            {"reserved", QListInt2QJsonArray(reserved)},
            {"allowed_ips", QListStr2QJsonArray({"0.0.0.0/0", "::/0"})},
            {"persistent_keepalive_interval", persistentKeepalive},
        };
        QJsonObject outbound{
            {"type", "wireguard"},
            {"name", tun_name},
            {"address", QListStr2QJsonArray(localAddress)},
            {"private_key", privateKey},
            {"peers", QJsonArray{peer}},
            {"mtu", MTU},
            {"system", useSystemInterface}
        };
        /*
        if (is_amnezia)
        {
            outbound["junk_packet_count"] = junk_packet_count;
            outbound["junk_packet_min_size"] = junk_packet_min_size;
            outbound["junk_packet_max_size"] = junk_packet_max_size;

            outbound["init_packet_junk_size"] = init_packet_junk_size;
            outbound["response_packet_junk_size"] = response_packet_junk_size;
            outbound["cookie_reply_junk_size"] = cookie_reply_junk_size;
            outbound["transport_packet_junk_size"] = transport_packet_junk_size;

            outbound["init_packet_magic_header"] = init_packet_magic_header;
            outbound["response_packet_magic_header"] = response_packet_magic_header;
            outbound["underload_packet_magic_header"] = cookie_reply_magic_header;
            outbound["transport_packet_magic_header"] = transport_packet_magic_header;
        }
        */

        result.outbound = outbound;
        return result;
    }

    bool WireguardBean::TryParseLink(const QString &link) {
        using namespace From_Link;
        if (link.startsWith("vpn://", Qt::CaseInsensitive)) return parseAmneziaVpnLink(link);
        if (parseWgConfig(link)) return true;

        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);
        add_default_fields(url, entity);

        privateKey = query.queryItemValue("private_key");
        publicKey = query.queryItemValue("peer_public_key");
        preSharedKey = query.queryItemValue("pre_shared_key");
        auto rawReserved = query.queryItemValue("reserved");
        if (!rawReserved.isEmpty()) {
            for (const auto &item: rawReserved.split("-")) reserved += item.toInt();
        }
        auto rawLocalAddr = query.queryItemValue("local_address");
        if (!rawLocalAddr.isEmpty()) {
            for (const auto &item: rawLocalAddr.split("-")) localAddress += item;
        }
        persistentKeepalive = GetQueryIntValue(query, "persistent_keepalive", GetQueryIntValue(query, "persistent_keepalive_interval"));
        MTU = GetQueryIntValue(query, "mtu");
        set_boolean("use_system_interface", useSystemInterface, query);

        junk_packet_count = GetQueryIntValue(query, "junk_packet_count");
        junk_packet_min_size = GetQueryIntValue(query, "junk_packet_min_size");
        junk_packet_max_size = GetQueryIntValue(query, "junk_packet_max_size");

        init_packet_junk_size = GetQueryIntValue(query, "init_packet_junk_size", GetQueryIntValue(query, "s1"));
        response_packet_junk_size = GetQueryIntValue(query, "response_packet_junk_size", GetQueryIntValue(query, "s2"));
        cookie_reply_junk_size = GetQueryIntValue(query, "underload_packet_junk_size", GetQueryIntValue(query, "s3"));
        transport_packet_junk_size = GetQueryIntValue(query, "transport_packet_junk_size", GetQueryIntValue(query, "s4"));

        init_packet_magic_header = GetQueryValue(query, "init_packet_magic_header", GetQueryValue(query, "h1"));
        response_packet_magic_header = GetQueryValue(query, "response_packet_magic_header", GetQueryValue(query, "h2"));
        cookie_reply_magic_header = GetQueryValue(query, "underload_packet_magic_header",
                                    GetQueryValue(query, "cookie_reply_magic_header",
                                    GetQueryValue(query, "h3")));
        transport_packet_magic_header = GetQueryValue(query, "transport_packet_magic_header", GetQueryValue(query, "h4"));

        i1 = GetQueryValue(query, "i1");
        i2 = GetQueryValue(query, "i2");
        i3 = GetQueryValue(query, "i3");
        i4 = GetQueryValue(query, "i4");
        i5 = GetQueryValue(query, "i5");

        bool enable_amnezia = false;
        set_boolean("enable_amnezia", enable_amnezia, query);
        if (!enable_amnezia){
            enable_amnezia = (junk_packet_count > 0 || junk_packet_min_size > 0 || junk_packet_max_size > 0)
                    || (init_packet_junk_size > 0 || response_packet_junk_size > 0)
                    || (cookie_reply_junk_size > 0 || transport_packet_junk_size > 0);
        }
        if (!enable_amnezia){
            auto scheme = url.scheme();
            enable_amnezia = (scheme == "awg" || scheme == "amneziawg" || scheme == "amnezia");
        }
        this->enableAmnezia(enable_amnezia);
        return true;
    }

    bool WireguardBean::TryParseJson(const Configs::Data::Node& obj)
    {
        using namespace Configs::From_Json;
        enableAmnezia(obj["type"].toString() == "awg");
        if (is_amnezia){
            return TryParseJsonAwg(obj);
        }
        add_default_fields(this->entity, obj);
        auto & peers = obj["peers"];
        if (!peers.isArray() || peers.count() == 0 ) return false;
        publicKey = peers[0]["public_key"].toString();
        reserved = peers[0]["reserved"].toIntList();
        persistentKeepalive = peers[0]["persistent_keepalive_interval"].toInt();
        privateKey = obj["private_key"].toString();
        localAddress = obj["address"].toStringList();
        MTU = obj["mtu"].toInt();
        useSystemInterface = obj["system"].toBool();

        return true;
    }

    void WireguardBean::enableAmnezia(bool enable){
        this->is_amnezia = enable;
        this->entity->type = this->type();
    };


    QString WireguardBean::type() const {
        return (!is_amnezia) ? "wireguard" : "awg";
    };


    QString WireguardBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        url.setScheme("wg");
        add_default_fields(url, this);
        QUrlQuery query;
        add_query_nonempty("private_key", query, privateKey);
        add_query_nonempty("peer_public_key", query, publicKey);
        add_query_nonempty("pre_shared_key", query, preSharedKey);
        add_query_nonempty("reserved", query, FormatReserved());
        add_query_int("persistent_keepalive", query, (persistentKeepalive));
        add_query_int("mtu", query, (MTU));
        add_query_boolean("use_system_interface", query, useSystemInterface);
        add_query_nonempty("local_address", query, localAddress.join("-"));
        if (is_amnezia)
        {
            query.addQueryItem("enable_amnezia", "true");
            add_query_int("junk_packet_count", query, (junk_packet_count));
            add_query_int("junk_packet_min_size", query, (junk_packet_min_size));
            add_query_int("junk_packet_max_size", query, (junk_packet_max_size));

            add_query_int("init_packet_junk_size", query, (init_packet_junk_size));
            add_query_int("response_packet_junk_size", query, (response_packet_junk_size));
            add_query_int("underload_packet_junk_size", query, (cookie_reply_junk_size));
            add_query_int("transport_packet_junk_size", query, (transport_packet_junk_size));

            add_query_nonempty("init_packet_magic_header", query, (init_packet_magic_header));
            add_query_nonempty("response_packet_magic_header", query, (response_packet_magic_header));
            add_query_nonempty("cookie_reply_magic_header", query, (cookie_reply_magic_header));
            add_query_nonempty("transport_packet_magic_header", query, (transport_packet_magic_header));

            add_query_nonempty("i1", query, (i1));
            add_query_nonempty("i2", query, (i2));
            add_query_nonempty("i3", query, (i3));
            add_query_nonempty("i4", query, (i4));
            add_query_nonempty("i5", query, (i5));
        }
        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }
    
        QString WireguardBean::FormatReserved() const {
            QString res = "";
            for (int i=0;i<reserved.size();i++) {
                res += QString::number(reserved[i]);
                if (i != reserved.size() - 1) {
                    res += "-";
                }
            }
            return res;
        }

        inline static bool startsWith(const QString & key, const QString & value){
            return QString::compare(key, value, Qt::CaseInsensitive) == 0;
        }

        bool WireguardBean::parseWgConfig(QString config_str)
        {
            QTextStream in(&config_str, QIODeviceBase::ReadOnly);
            iniqt::Ini ini;
            ini.parse(in);
            auto & config = ini.sections;

            bool enable_amnezia = false;
            if (!config.contains("Interface") || !config.contains("Peer")) return false;
            auto line_parser  = [&enable_amnezia, this](const QString &key, const QString & value) -> bool {
                if (key.trimmed().isEmpty()) return true;
                if (startsWith(key, "PrivateKey"))
                {
                    privateKey = value.trimmed();
                }
                if (startsWith(key, "Address"))
                {
                    auto addresses = value.trimmed().split(",");
                    for (const auto& address : addresses) localAddress.append(address.trimmed());
                }
                if (startsWith(key, "MTU"))
                {
                    MTU = value.toInt();
                }
                if (startsWith(key, "PublicKey"))
                {
                    publicKey = value.trimmed();
                }
                if (startsWith(key, "PresharedKey"))
                {
                    preSharedKey = value.trimmed();
                }
                if (startsWith(key, "PersistentKeepalive"))
                {
                    persistentKeepalive = value.toInt();
                }
                if (startsWith(key, "Endpoint"))
                {
                    auto addrPort = value.trimmed();
                    if (!addrPort.contains(":")) return false;
                    entity->serverAddress = addrPort.split(":")[0].trimmed();
                    entity->serverPort = addrPort.split(":")[1].trimmed().toInt();
                }
                if (startsWith(key, "S1"))
                {
                    enable_amnezia = true;
                    init_packet_junk_size = value.toInt();
                }
                if (startsWith(key, "S2"))
                {
                    enable_amnezia = true;
                    response_packet_junk_size = value.toInt();
                }
                if (startsWith(key, "S3"))
                {
                    enable_amnezia = true;
                    cookie_reply_junk_size = value.toInt();
                }
                if (startsWith(key, "S4"))
                {
                    enable_amnezia = true;
                    transport_packet_junk_size = value.toInt();
                }
                if (startsWith(key, "Jc"))
                {
                    enable_amnezia = true;
                    junk_packet_count = value.toInt();
                }
                if (startsWith(key, "Jmin"))
                {
                    enable_amnezia = true;
                    junk_packet_min_size = value.toInt();
                }
                if (startsWith(key, "Jmax"))
                {
                    enable_amnezia = true;
                    junk_packet_max_size = value.toInt();
                }
                if (startsWith(key, "H1"))
                {
                    enable_amnezia = true;
                    init_packet_magic_header = value;
                }
                if (startsWith(key, "H2"))
                {
                    enable_amnezia = true;
                    response_packet_magic_header = value;
                }
                if (startsWith(key, "H3"))
                {
                    enable_amnezia = true;
                    cookie_reply_magic_header = value;
                }
                if (startsWith(key, "H4"))
                {
                    enable_amnezia = true;
                    transport_packet_magic_header = value;
                }

                if (startsWith(key, "I1"))
                {
                    enable_amnezia = true;
                    i1 = value;
                }
                if (startsWith(key, "I2"))
                {
                    enable_amnezia = true;
                    i2 = value;
                }
                if (startsWith(key, "I3"))
                {
                    enable_amnezia = true;
                    i3 = value;
                }
                if (startsWith(key, "I4"))
                {
                    enable_amnezia = true;
                    i4 = value;
                }
                if (startsWith(key, "I5"))
                {
                    enable_amnezia = true;
                    i5 = value;
                }
                return true;
            };
            for (auto & map : config.values()){
                for (auto [key, value]: asKeyValueRange(map)){
                    line_parser(key, value);
                }
            }

            this->enableAmnezia(enable_amnezia);
            entity->name = "Wg file config";
            return true;
        };
}

namespace Configs {

CoreObjOutboundBuildResult WireguardBean::BuildCoreObjSingBoxAwg() const {
    CoreObjOutboundBuildResult result;
    using namespace To_CoreObj_box;

    QJsonArray peers;

        QJsonObject peer{
            {"address", entity->serverAddress},
            {"port", entity->serverPort},
            {"public_key", publicKey},
            {"pre_shared_key", preSharedKey},
            {"allowed_ips", QListStr2QJsonArray({"0.0.0.0/0", "::/0"})},
            {"persistent_keepalive_interval", persistentKeepalive},
        };

        peers << peer;

    QJsonObject outbound{
        {"type", "awg"},
        {"address", QListStr2QJsonArray(localAddress)},
        {"private_key", privateKey},
        {"peers", peers},
        {"mtu", MTU},
//        {"listen_port", entity->serverPort},
        {"useIntegratedTun", useSystemInterface}
    };

    // AmneziaWG params
    {
        outbound["jc"] = junk_packet_count;
        outbound["jmin"] = junk_packet_min_size;
        outbound["jmax"] = junk_packet_max_size;

        outbound["s1"] = init_packet_junk_size;
        outbound["s2"] = response_packet_junk_size;
        outbound["s3"] = cookie_reply_junk_size;
        outbound["s4"] = transport_packet_junk_size;

        outbound["h1"] = init_packet_magic_header;
        outbound["h2"] = response_packet_magic_header;
        outbound["h3"] = cookie_reply_magic_header;
        outbound["h4"] = transport_packet_magic_header;

        outbound["i1"] = i1;
        outbound["i2"] = i2;
        outbound["i3"] = i3;
        outbound["i4"] = i4;
        outbound["i5"] = i5;
    }

    result.outbound = outbound;
    return result;
}

// -------------------- JSON PARSER --------------------

bool WireguardBean::TryParseJsonAwg(const Configs::Data::Node &obj) {
    using namespace Configs::From_Json;

    // -------------------- Basic fields --------------------
    privateKey = obj["private_key"].toString();

    localAddress = obj["address"].toStringList();

    MTU = obj["mtu"].toInt();

    useSystemInterface = obj["useIntegratedTun"].toBool();

    // -------------------- Peers --------------------
    auto &peersArray = obj["peers"];
    auto &peerObj = peersArray[0];

    entity->serverAddress = peerObj["address"].toString();
    entity->serverPort = peerObj["port"].toInt();
    this->publicKey = peerObj["public_key"].toString();
    this->preSharedKey = peerObj["pre_shared_key"].toString();
    this->persistentKeepalive =
            peerObj["persistent_keepalive_interval"].toInt();

    // -------------------- AmneziaWG params --------------------
    junk_packet_count = obj["jc"].toInt();
    this->junk_packet_max_size = obj["jmax"].toInt();
    this->junk_packet_min_size = obj["jmin"].toInt();

    this->init_packet_junk_size = obj["s1"].toInt();
    this->response_packet_junk_size = obj["s2"].toInt();
    this->cookie_reply_junk_size = obj["s3"].toInt();
    this->transport_packet_junk_size = obj["s4"].toInt();

    this->init_packet_magic_header = obj["h1"].toString();
    this->response_packet_magic_header = obj["h2"].toString();
    this->cookie_reply_magic_header = obj["h3"].toString();
    this->transport_packet_magic_header = obj["h4"].toString();

    i1 = obj["i1"].toString();
    i2 = obj["i2"].toString();
    i3 = obj["i3"].toString();
    i4 = obj["i4"].toString();
    i5 = obj["i5"].toString();

    return true;
}

// -------------------- AMNEZIA vpn:// LINK --------------------

// a header spec is "5" or a "5-10" range; blank means the standard message type
static bool parseMagicHeader(const QString &spec, qint64 standard, qint64 &begin, qint64 &end) {
    const auto trimmed = spec.trimmed();
    if (trimmed.isEmpty()) {
        begin = end = standard;
        return true;
    }
    const auto parts = trimmed.split("-");
    if (parts.size() > 2) return false;
    bool ok = false;
    begin = parts[0].trimmed().toLongLong(&ok);
    if (!ok) return false;
    end = begin;
    if (parts.size() == 2) {
        end = parts[1].trimmed().toLongLong(&ok);
        if (!ok) return false;
    }
    return begin >= 0 && end <= 0xFFFFFFFFLL && end >= begin;
}

// the core draws junk sizes with rand.Int(jmax - jmin + 1) and panics on a non-positive argument
bool WireguardBean::ValidateAmnezia() const {
    if (junk_packet_count > 0 && (junk_packet_min_size <= 0 || junk_packet_max_size <= 0)) return false;
    if (junk_packet_min_size > 0 && junk_packet_max_size > 0
        && junk_packet_min_size > junk_packet_max_size) return false;

    // unset headers fall back to standard message types, so one custom header can still collide
    const QString specs[4] = {init_packet_magic_header, response_packet_magic_header,
                              cookie_reply_magic_header, transport_packet_magic_header};
    qint64 begin[4], end[4];
    for (int i = 0; i < 4; i++) {
        if (!parseMagicHeader(specs[i], i + 1, begin[i], end[i])) return false;
    }
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            if (begin[i] <= end[j] && begin[j] <= end[i]) return false;
        }
    }
    return true;
}

// vpn:// + base64url(4-byte BE length + zlib(JSON)), the framing of Qt's own qCompress
bool WireguardBean::parseAmneziaVpnLink(const QString &link) {
    auto payload = SubStrBefore(SubStrAfter(link, "://"), "#").trimmed();
    payload.replace('-', '+').replace('_', '/');
    const auto plain = qUncompress(QByteArray::fromBase64(payload.toUtf8()));
    if (plain.isEmpty()) return false;

    const auto root = QJsonDocument::fromJson(plain).object();
    QJsonObject awg;
    for (const auto &container : root["containers"].toArray()) {
        auto obj = container.toObject()["awg"].toObject();
        if (!obj.isEmpty()) {
            awg = obj;
            break;
        }
    }
    if (awg.isEmpty()) return false;

    const auto last = QJsonDocument::fromJson(awg["last_config"].toString().toUtf8()).object();

    // numbers travel as strings in some Amnezia versions and as ints in others
    auto jsonStr = [&](const char *key) {
        auto value = last[key];
        if (value.isUndefined() || value.isNull()) value = awg[key];
        return value.isDouble() ? QString::number(value.toInt()) : value.toString();
    };
    auto jsonInt = [&](const char *key) {
        auto value = last[key];
        if (value.isUndefined() || value.isNull()) value = awg[key];
        return value.isString() ? value.toString().trimmed().toInt() : value.toInt();
    };

    // the .conf is the source of truth, the JSON fields only fill what it omits
    MTU = 0;
    parseWgConfig(last["config"].toString());

    if (entity->serverAddress.isEmpty()) {
        entity->serverAddress = jsonStr("hostName");
        if (entity->serverAddress.isEmpty()) entity->serverAddress = root["hostName"].toString();
    }
    if (entity->serverPort <= 0) entity->serverPort = jsonInt("port");
    if (entity->serverPort <= 0) entity->serverPort = 51820;
    if (localAddress.isEmpty()) {
        const auto client_ip = jsonStr("client_ip");
        if (!client_ip.isEmpty()) localAddress << client_ip;
    }
    if (privateKey.isEmpty()) privateKey = jsonStr("client_priv_key");
    if (publicKey.isEmpty()) publicKey = jsonStr("server_pub_key");
    if (preSharedKey.isEmpty()) preSharedKey = jsonStr("psk_key");
    if (MTU <= 0) MTU = jsonInt("mtu");
    if (MTU <= 0) MTU = 1420;
    if (persistentKeepalive <= 0) persistentKeepalive = jsonInt("persistent_keep_alive");

    if (junk_packet_count <= 0) junk_packet_count = jsonInt("Jc");
    if (junk_packet_min_size <= 0) junk_packet_min_size = jsonInt("Jmin");
    if (junk_packet_max_size <= 0) junk_packet_max_size = jsonInt("Jmax");
    if (init_packet_junk_size <= 0) init_packet_junk_size = jsonInt("S1");
    if (response_packet_junk_size <= 0) response_packet_junk_size = jsonInt("S2");
    if (cookie_reply_junk_size <= 0) cookie_reply_junk_size = jsonInt("S3");
    if (transport_packet_junk_size <= 0) transport_packet_junk_size = jsonInt("S4");

    if (init_packet_magic_header.isEmpty()) init_packet_magic_header = jsonStr("H1");
    if (response_packet_magic_header.isEmpty()) response_packet_magic_header = jsonStr("H2");
    if (cookie_reply_magic_header.isEmpty()) cookie_reply_magic_header = jsonStr("H3");
    if (transport_packet_magic_header.isEmpty()) transport_packet_magic_header = jsonStr("H4");

    if (i1.isEmpty()) i1 = jsonStr("I1");
    if (i2.isEmpty()) i2 = jsonStr("I2");
    if (i3.isEmpty()) i3 = jsonStr("I3");
    if (i4.isEmpty()) i4 = jsonStr("I4");
    if (i5.isEmpty()) i5 = jsonStr("I5");

    if (privateKey.isEmpty() || publicKey.isEmpty() || entity->serverAddress.isEmpty()) return false;

    auto name = root["description"].toString();
    if (name.isEmpty() && link.contains("#")) {
        name = QUrl::fromPercentEncoding(SubStrAfter(link, "#").toUtf8());
    }
    entity->name = name.isEmpty() ? "AmneziaWG" : name;

    // an awg container is always AmneziaWG, even with no obfuscation knobs to detect
    enableAmnezia(true);
    return ValidateAmnezia();
}

}
