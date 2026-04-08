#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/configs/proxy/V2RayStreamSettings.hpp>
#include <nekobox/dataStore/ProfileFilter.hpp>
#include <nekobox/dataStore/Utils.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <nekobox/global/HTTPRequestHelper.hpp>
#include <nekobox/configs/sub/GroupUpdater.hpp>
#include <QMutex>
#include <3rdparty/fkYAML/node.hpp>
#include <nekobox/configs/ConfigBuilder.hpp>
#include <QThreadPool>


namespace Subscription {

    GroupUpdater *groupUpdater = new GroupUpdater;

    void RawUpdater_FixEnt(const std::shared_ptr<Configs::ProxyEntity> &ent) {
        auto bean = ent->unlock(ent->bean());
        if (ent == nullptr) return;
        auto stream = Configs::GetStreamSettings(bean.get());
        if (stream == nullptr) return;
        // 1. "security"
        if (stream->security == "none" || stream->security == "0" || stream->security == "false") {
            stream->security = "";
        } else if (stream->security == "1" || stream->security == "true") {
            stream->security = "tls";
        }
        // 2. TLS SNI: v2rayN config builder generate sni like this, so set sni here for their format.
        if (stream->security == "tls" && IsIpAddress(ent->serverAddress) && (!stream->host.isEmpty()) && stream->sni.isEmpty()) {
            stream->sni = stream->host;
        }
    }

    int JsonEndIdx(const QString &str, int begin) {
        int sz = str.length();
        int counter = 1;
        for (int i=begin+1;i<sz;i++) {
            if (str[i] == '{') counter++;
            if (str[i] == '}') counter--;
            if (counter==0) return i;
        }
        return -1;
    }

    QList<QString> Disect(const QString &str) {
        QList<QString> res = QList<QString>();
        int idx=0;
        int sz = str.size();
        while(idx < sz) {
            if (str[idx] == '\n') {
                idx++;
                continue;
            }
            if (str[idx] == '{') {
                int endIdx = JsonEndIdx(str, idx);
                if (endIdx == -1) return res;
                res.append(str.mid(idx, endIdx-idx + 1));
                idx = endIdx+1;
                continue;
            }
            int nlineIdx = str.indexOf('\n', idx);
            if (nlineIdx == -1) nlineIdx = sz;
            res.append(str.mid(idx, nlineIdx-idx));
            idx = nlineIdx+1;
        }
        return res;
    }

    void RawUpdater::update(const QString &str, bool needParse = true) {
        // Base64 encoded subscription
        if (auto str2 = DecodeB64IfValid(str); !str2.isEmpty()) {
            update(str2);
            return;
        }

        // Clash
        if (str.contains("proxies:")) {
            updateClash(str);
            return;
        }

        // SingBox
        if (str.contains("outbounds") || str.contains("endpoints"))
        {
            updateSingBox(str);
            return;
        }

        // Wireguard Config
        if (str.contains("[Interface]") && str.contains("[Peer]"))
        {
            updateWireguardFileConfig(str);
            return;
        }

        // SIP008
        if (str.contains("version") && str.contains("servers"))
        {
            updateSIP008(str);
            return;
        }

        // Multi line
        if (str.count("\n") > 0 && needParse) {
            auto list = Disect(str);
            for (const auto &str2: list) {
                update(str2.trimmed(), false);
            }
            return;
        }

        // is comment or too short
        if (str.startsWith("//") || str.startsWith("#") || str.length() < 2) {
            return;
        }

        std::shared_ptr<Configs::ProxyEntity> ent;
        bool needFix = true;

        // Nekoray format
        if (str.startsWith("nekoray://")) {
            needFix = false;
            auto link = QUrl(str);
            if (!link.isValid()) return;
            ent = Configs::ProfileManager::NewProxyEntity(link.host());
            if (!ent->isValid()) return;
            auto j = DecodeB64IfValid(link.fragment().toUtf8(), QByteArray::Base64UrlEncoding);
            if (j.isEmpty()) return;
            ent->unlock(ent->bean())->FromJsonBytes(j);
        }

        // Json
        if (str.startsWith('{')) {
            ent = Configs::ProfileManager::NewProxyEntity("custom");
            auto bean = ent->unlock(ent->CustomBean());
            auto obj = QString2QJsonObject(str);
            if (obj.contains("outbounds")) {
                bean->core = "internal-full";
                bean->config_simple = str;
            } else if (obj.contains("server")) {
                bean->core = "internal";
                bean->config_simple = str;
            } else {
                return;
            }
        }

        // SOCKS
        if (str.startsWith("socks5://") || str.startsWith("socks4://") ||
            str.startsWith("socks4a://") || str.startsWith("socks://")) {
            ent = Configs::ProfileManager::NewProxyEntity("socks");
            auto ok = ent->unlock(ent->SocksBean())->TryParseLink(str);
            if (!ok) return;
        }

        // HTTP
        if (str.startsWith("http://") || str.startsWith("https://")) {
            ent = Configs::ProfileManager::NewProxyEntity("http");
            auto ok = ent->unlock(ent->HttpBean())->TryParseLink(str);
            if (!ok) return;
        }

        // ShadowSocks
        if (str.startsWith("ss://")) {
            ent = Configs::ProfileManager::NewProxyEntity("shadowsocks");
            auto ok = ent->unlock(ent->ShadowSocksBean())->TryParseLink(str);
            if (!ok) return;
        }

        // VMess
        if (str.startsWith("vmess://")) {
            ent = Configs::ProfileManager::NewProxyEntity("vmess");
            auto ok = ent->unlock(ent->VMessBean())->TryParseLink(str);
            if (!ok) return;
        }

        // VLESS
        if (str.startsWith("vless://")) {
            ent = Configs::ProfileManager::NewProxyEntity("vless");
            auto ok = ent->unlock(ent->TrojanVLESSBean())->TryParseLink(str);
            if (!ok) return;
        }

        // Trojan
        if (str.startsWith("trojan://")) {
            ent = Configs::ProfileManager::NewProxyEntity("trojan");
            auto ok = ent->unlock(ent->TrojanVLESSBean())->TryParseLink(str);
            if (!ok) return;
        }

        // Mieru
        if (str.startsWith("mierus://") || str.startsWith("mieru://")) {
            ent = Configs::ProfileManager::NewProxyEntity("mieru");
            auto ok = ent->unlock(ent->MieruBean())->TryParseLink(str);
            if (!ok) return;
        }

        // Naive
        {
            bool quic_enabled = false;
            if (
                (str.startsWith("naive://") || str.startsWith("naive+https://") || str.startsWith("naive+http://")) ||
                (quic_enabled = str.startsWith("naive+quic://"))
            ) {
                ent = Configs::ProfileManager::NewProxyEntity("naive");
                auto bean = ent->unlock(ent->NaiveBean());
                if (quic_enabled){
                    bean->quic = true;
                    *bean->quic_congestion_control = 1;
                }
                auto ok = bean->TryParseLink(str);
                if (!ok) return;
            }
        }

        // TrustTunnel
        if (str.startsWith("tt://")) {
            ent = Configs::ProfileManager::NewProxyEntity("trusttunnel");
            auto ok = ent->unlock(ent->TrustTunnelBean())->TryParseLink(str);
            if (!ok) return;
        }

        // Juicity
        if (str.startsWith("juicity://")) {
            ent = Configs::ProfileManager::NewProxyEntity("juicity");
            auto ok = ent->unlock(ent->JuicityBean())->TryParseLink(str);
            if (!ok) return;
        }

        // AnyTLS
        if (str.startsWith("anytls://")) {
            ent = Configs::ProfileManager::NewProxyEntity("anytls");
            auto ok = ent->unlock(ent->AnyTLSBean())->TryParseLink(str);
            if (!ok) return;
        }

        // ShadowTLS
        if (str.startsWith("shadowtls://")) {
            ent = Configs::ProfileManager::NewProxyEntity("shadowtls");
            auto ok = ent->unlock(ent->ShadowTLSBean())->TryParseLink(str);
            if (!ok) return;
        }

        // Hysteria1
        if (str.startsWith("hysteria://")) {
            needFix = false;
            ent = Configs::ProfileManager::NewProxyEntity("hysteria");
            auto ok = ent->unlock(ent->QUICBean())->TryParseLink(str);
            if (!ok) return;
        }

        // Hysteria2
        if (str.startsWith("hysteria2://") || str.startsWith("hy2://")) {
            needFix = false;
            ent = Configs::ProfileManager::NewProxyEntity("hysteria2");
            auto ok = ent->unlock(ent->QUICBean())->TryParseLink(str);
            if (!ok) return;
        }

        // TUIC
        if (str.startsWith("tuic://")) {
            needFix = false;
            ent = Configs::ProfileManager::NewProxyEntity("tuic");
            auto ok = ent->unlock(ent->QUICBean())->TryParseLink(str);
            if (!ok) return;
        }

        // Wireguard
        if (str.startsWith("wg://")) {
            needFix = false;
            ent = Configs::ProfileManager::NewProxyEntity("wireguard");
            auto ok = ent->unlock(ent->WireguardBean())->TryParseLink(str);
            if (!ok) return;
        }

        // SSH
        if (str.startsWith("ssh://")) {
            needFix = false;
            ent = Configs::ProfileManager::NewProxyEntity("ssh");
            auto ok = ent->unlock(ent->SSHBean())->TryParseLink(str);
            if (!ok) return;
        }

        // tor
        if (str.startsWith("tor://")) {
            needFix = false;
            ent = Configs::ProfileManager::NewProxyEntity("tor");
            auto ok = ent->unlock(ent->TorBean())->TryParseLink(str);
            if (!ok) return;
        }

        if (ent == nullptr) return;

        // Fix
        if (needFix) RawUpdater_FixEnt(ent);

        // End
        updated_order += ent;
    }

    void RawUpdater::updateSIP008(const QString& str)
    {
        auto json = QString2QJsonObject(str);

        for (auto o : json["servers"].toArray())
        {
            auto out = o.toObject();
            if (out.isEmpty())
            {
                MW_show_log("invalid server object");
                continue;
            }

            auto ent = Configs::ProfileManager::NewProxyEntity("shadowsocks");
            auto ok = ent->unlock(ent->ShadowSocksBean())->TryParseFromSIP008(out);
            if (!ok) continue;
            updated_order += ent;
        }
    }

    void RawUpdater::updateSingBox(const QString& str)
    {
        auto json = QString2QJsonObject(str);
        auto outbounds = json["outbounds"].toArray();
        auto endpoints = json["endpoints"].toArray();
        QJsonArray items;
        for (auto && outbound : outbounds)
        {
            if (!outbound.isObject()) continue;
            items.append(outbound.toObject());
        }
        for (auto && endpoint : endpoints)
        {
            if (!endpoint.isObject()) continue;
            items.append(endpoint.toObject());
        }

        for (auto o : items)
        {
            auto out = o.toObject();
            if (out.isEmpty())
            {
                MW_show_log(
                    QString("invalid outbound of type: ") +
                    QJsonType2QString(o.type()));
                continue;
            }

            std::shared_ptr<Configs::ProxyEntity> ent;
            QString out_type = out["type"].toString();

            // All Types
            if (Preset::SingBox::OutboundTypes.contains(out_type)) {
                ent = Configs::ProfileManager::NewProxyEntity(out_type);
                auto ok = ent->unlock(ent->bean())->TryParseJson(out);
                if (!ok) continue;
            }

            if (ent == nullptr) continue;

            updated_order += ent;
        }
    }

    void RawUpdater::updateWireguardFileConfig(const QString& str)
    {
        auto ent = Configs::ProfileManager::NewProxyEntity("wireguard");
        auto ok = ent->unlock(ent->WireguardBean())->TryParseLink(str);
        if (!ok) return;
        updated_order += ent;
    }


    QString Node2QString(const fkyaml::node &n, const QString &def = "") {
        try {
            return n.as_str().c_str();
        } catch (const fkyaml::exception &ex) {
            qDebug() << ex.what();
            return def;
        }
    }

    QStringList Node2QStringList(const fkyaml::node &n) {
        try {
            if (n.is_sequence()) {
                QStringList list;
                for (auto item: n) {
                    list << item.as_str().c_str();
                }
                return list;
            } else {
                return {};
            }
        } catch (const fkyaml::exception &ex) {
            qDebug() << ex.what();
            return {};
        }
    }

    int Node2Int(const fkyaml::node &n, const int &def = 0) {
        try {
            if (n.is_integer())
                return n.as_int();
            else if (n.is_string())
                return atoi(n.as_str().c_str());
            return def;
        } catch (const fkyaml::exception &ex) {
            qDebug() << ex.what();
            return def;
        }
    }

    bool Node2Bool(const fkyaml::node &n, const bool &def = false) {
        try {
            return n.as_bool();
        } catch (const fkyaml::exception &ex) {
            try {
                return n.as_int();
            } catch (const fkyaml::exception &ex2) {
                ex2.what();
            }
            qDebug() << ex.what();
            return def;
        }
    }

    // NodeChild returns the first defined children or Null Node
    fkyaml::node NodeChild(const fkyaml::node &n, const std::list<std::string> &keys) {
        for (const auto &key: keys) {
            if (n.contains(key)) return n[key];
        }
        return {};
    }

    // https://github.com/Dreamacro/clash/wiki/configuration
    void RawUpdater::updateClash(const QString &str) {
        try {
            auto proxies = fkyaml::node::deserialize(str.toStdString())["proxies"];
            for (auto proxy: proxies) {
                auto type = Node2QString(proxy["type"]).toLower();
                auto type_clash = type;

                if (type == "ss" || type == "ssr") type = "shadowsocks";
                if (type == "socks5") type = "socks";

                auto ent = Configs::ProfileManager::NewProxyEntity(type);
                if (!ent->isValid()) continue;
                bool needFix = false;

                // common
                ent->name = Node2QString(proxy["name"]);
                ent->serverAddress = Node2QString(proxy["server"]);
                ent->serverPort = Node2Int(proxy["port"]);

                if (type_clash == "ss") {
                    auto bean = ent->unlock(ent->ShadowSocksBean());
                    bean->method = Node2QString(proxy["cipher"]).replace("dummy", "none");
                    bean->password = Node2QString(proxy["password"]);

                    // UDP over TCP
                    if (Node2Bool(proxy["udp-over-tcp"])) {
                        bean->uot = Node2Int(proxy["udp-over-tcp-version"]);
                        if (bean->uot == 0) bean->uot = 2;
                    }  
                    *bean->network = Node2QString(proxy["network"]);

                    if (proxy.contains("plugin") && proxy.contains("plugin-opts")) {
                        auto plugin_n = proxy["plugin"];
                        auto pluginOpts_n = proxy["plugin-opts"];
                        QStringList ssPlugin;
                        auto plugin = Node2QString(plugin_n);
                        if (plugin == "obfs") {
                            ssPlugin << "obfs-local";
                            ssPlugin << "obfs=" + Node2QString(pluginOpts_n["mode"]);
                            ssPlugin << "obfs-host=" + Node2QString(pluginOpts_n["host"]);
                        } else if (plugin == "v2ray-plugin") {
                            auto mode = Node2QString(pluginOpts_n["mode"]);
                            auto host = Node2QString(pluginOpts_n["host"]);
                            auto path = Node2QString(pluginOpts_n["path"]);
                            ssPlugin << "v2ray-plugin";
                            if (!mode.isEmpty() && mode != "websocket") ssPlugin << "mode=" + mode;
                            if (Node2Bool(pluginOpts_n["tls"])) ssPlugin << "tls";
                            if (!host.isEmpty()) ssPlugin << "host=" + host;
                            if (!path.isEmpty()) ssPlugin << "path=" + path;
                            // clash only: skip-cert-verify
                            // clash only: headers
                            // clash: mux=?
                        }
                        bean->plugin = ssPlugin.join(";");
                    }

                    // sing-mux
                    auto smux = NodeChild(proxy, {"smux"});
                    if (!smux.is_null() && Node2Bool(smux["enabled"])) bean->mux_state = 1;
                    bean.reset();
                } else if (type == "socks") {
                    auto bean = ent->unlock(ent->SocksBean());
                    bean->username = Node2QString(proxy["username"]);
                    bean->password = Node2QString(proxy["password"]);
                    bean.reset();
                } else if (type == "http") {
                    auto bean = ent->unlock(ent->HttpBean());
                    bean->username = Node2QString(proxy["username"]);
                    bean->password = Node2QString(proxy["password"]);
                    if (type == "http" && Node2Bool(proxy["tls"])) {
                        bean->stream->security = "tls";
                        if (Node2Bool(proxy["skip-cert-verify"])) bean->stream->allow_insecure = true;
                        bean->stream->sni = FIRST_OR_SECOND(Node2QString(proxy["sni"]), Node2QString(proxy["servername"]));
                        bean->stream->alpn = Node2QStringList(proxy["alpn"]).join(",");
                        bean->stream->utlsFingerprint = Node2QString(proxy["client-fingerprint"]);
                        if (bean->stream->utlsFingerprint.isEmpty()) {
                            bean->stream->utlsFingerprint = Configs::dataStore->utlsFingerprint;
                        }

                        auto reality = NodeChild(proxy, {"reality-opts"});
                        if (reality.is_mapping()) {
                            bean->stream->reality_pbk = Node2QString(reality["public-key"]);
                            bean->stream->reality_sid = Node2QString(reality["short-id"]);
                        }
                    }
                    bean.reset();
                } else if (type == "trojan" || type == "vless") {
                    needFix = true;
                    auto bean = ent->unlock(ent->TrojanVLESSBean());
                    if (type == "vless") {
                        bean->flow = Node2QString(proxy["flow"]);
                        bean->password = Node2QString(proxy["uuid"]);
                        // meta packet encoding
                        if (Node2Bool(proxy["packet-addr"])) {
                            bean->stream->packet_encoding = "packetaddr";
                        } else {
                            // For VLESS, default to use xudp
                            bean->stream->packet_encoding = "xudp";
                        }
                    } else {
                        bean->password = Node2QString(proxy["password"]);
                    }
                    bean->stream->security = "tls";
                    bean->stream->network = Node2QString(proxy["network"], "tcp");
                    bean->stream->sni = FIRST_OR_SECOND(Node2QString(proxy["sni"]), Node2QString(proxy["servername"]));
                    bean->stream->alpn = Node2QStringList(proxy["alpn"]).join(",");
                    bean->stream->allow_insecure = Node2Bool(proxy["skip-cert-verify"]);
                    bean->stream->utlsFingerprint = Node2QString(proxy["client-fingerprint"]);
                    if (bean->stream->utlsFingerprint.isEmpty()) {
                        bean->stream->utlsFingerprint = Configs::dataStore->utlsFingerprint;
                    }

                    // sing-mux
                    auto smux = NodeChild(proxy, {"smux"});
                    if (!smux.is_null() && Node2Bool(smux["enabled"])) bean->mux_state = 1;

                    // opts
                    auto ws = NodeChild(proxy, {"ws-opts", "ws-opt"});
                    if (ws.is_mapping()) {
                        auto headers = ws["headers"];
                        if (headers.is_mapping()) {
                            for (auto header: headers.as_map()) {
                                if (Node2QString(header.first).toLower() == "host") {
                                    if (header.second.is_string())
                                        bean->stream->host = Node2QString(header.second);
                                    else if (header.second.is_sequence() && header.second[0].is_string())
                                        bean->stream->host = Node2QString(header.second[0]);
                                    break;
                                }
                            }
                        }
                        bean->stream->path = Node2QString(ws["path"]);
                        bean->stream->ws_early_data_length = Node2Int(ws["max-early-data"]);
                        bean->stream->ws_early_data_name = Node2QString(ws["early-data-header-name"]);
                    }

                    auto grpc = NodeChild(proxy, {"grpc-opts", "grpc-opt"});
                    if (grpc.is_mapping()) {
                        bean->stream->path = Node2QString(grpc["grpc-service-name"]);
                    }

                    auto reality = NodeChild(proxy, {"reality-opts"});
                    if (reality.is_mapping()) {
                        bean->stream->reality_pbk = Node2QString(reality["public-key"]);
                        bean->stream->reality_sid = Node2QString(reality["short-id"]);
                    }
                    bean.reset();
                } else if (type == "vmess") {
                    needFix = true;
                    auto bean = ent->unlock(ent->VMessBean());
                    bean->uuid = Node2QString(proxy["uuid"]);
                    bean->aid = Node2Int(proxy["alterId"]);
                    bean->security = Node2QString(proxy["cipher"], bean->security);
                    bean->stream->network = Node2QString(proxy["network"], "tcp").replace("h2", "http");
                    bean->stream->sni = FIRST_OR_SECOND(Node2QString(proxy["sni"]), Node2QString(proxy["servername"]));
                    bean->stream->alpn = Node2QStringList(proxy["alpn"]).join(",");
                    if (Node2Bool(proxy["tls"])) bean->stream->security = "tls";
                    if (Node2Bool(proxy["skip-cert-verify"])) bean->stream->allow_insecure = true;
                    bean->stream->utlsFingerprint = Node2QString(proxy["client-fingerprint"]);
                    if (bean->stream->utlsFingerprint.isEmpty()) {
                        bean->stream->utlsFingerprint = Configs::dataStore->utlsFingerprint;
                    }

                    // sing-mux
                    auto smux = NodeChild(proxy, {"smux"});
                    if (!smux.is_null() && Node2Bool(smux["enabled"])) bean->mux_state = 1;

                    // meta packet encoding
                    if (Node2Bool(proxy["xudp"])) bean->stream->packet_encoding = "xudp";
                    if (Node2Bool(proxy["packet-addr"])) bean->stream->packet_encoding = "packetaddr";

                    // opts
                    auto ws = NodeChild(proxy, {"ws-opts", "ws-opt"});
                    if (ws.is_mapping()) {
                        auto headers = ws["headers"];
                        if (headers.is_mapping()) {
                            for (auto header: headers.as_map()) {
                                if (Node2QString(header.first).toLower() == "host") {
                                    bean->stream->host = Node2QString(header.second);
                                    break;
                                }
                            }
                        }
                        bean->stream->path = Node2QString(ws["path"]);
                        bean->stream->ws_early_data_length = Node2Int(ws["max-early-data"]);
                        bean->stream->ws_early_data_name = Node2QString(ws["early-data-header-name"]);
                        // for Xray
                        if (Node2QString(ws["early-data-header-name"]) == "Sec-WebSocket-Protocol") {
                            bean->stream->path += "?ed=" + Node2QString(ws["max-early-data"]);
                        }
                    }

                    auto grpc = NodeChild(proxy, {"grpc-opts", "grpc-opt"});
                    if (grpc.is_mapping()) {
                        bean->stream->path = Node2QString(grpc["grpc-service-name"]);
                    }

                    auto h2 = NodeChild(proxy, {"h2-opts", "h2-opt"});
                    if (h2.is_mapping()) {
                        auto hosts = h2["host"];
                        for (auto host: hosts) {
                            bean->stream->host = Node2QString(host);
                            break;
                        }
                        bean->stream->path = Node2QString(h2["path"]);
                    }
                    auto tcp_http = NodeChild(proxy, {"http-opts", "http-opt"});
                    if (tcp_http.is_mapping()) {
                        bean->stream->network = "tcp";
                        bean->stream->header_type = "http";
                        auto headers = tcp_http["headers"];
                        if (headers.is_mapping()) {
                            for (auto header: headers.as_map()) {
                                if (Node2QString(header.first).toLower() == "host") {
                                    bean->stream->host = Node2QString(header.second);
                                    break;
                                }
                            }
                        }
                        auto paths = tcp_http["path"];
                        if (paths.is_string())
                            bean->stream->path = Node2QString(paths);
                        else if (paths.is_sequence() && paths[0].is_string())
                            bean->stream->path = Node2QString(paths[0]);
                    }
                    bean.reset();
                } else if (type == "anytls" || type == "shadowtls") {
                    std::shared_ptr<Configs::AbstractBean> bean_common;
                    needFix = true;
                    std::shared_ptr<Configs::V2rayStreamSettings> stream;
                    if (type == "anytls"){
                        auto bean = ent->unlock(ent->AnyTLSBean());
                        bean->password = Node2QString(proxy["password"]);
                        stream = bean->stream;
                        bean_common = bean;
                    } else {
                        auto bean = ent->unlock(ent->ShadowTLSBean());
                        bean->password = Node2QString(proxy["password"]);
                        bean->shadowtls_version = Node2Int(proxy["version"]);
                        stream = bean->stream;
                        bean_common = bean;
                    }
                    stream->security = "tls";
                    if (Node2Bool(proxy["skip-cert-verify"])) stream->allow_insecure = true;
                    stream->sni = FIRST_OR_SECOND(Node2QString(proxy["sni"]), Node2QString(proxy["servername"]));
                    stream->alpn = Node2QStringList(proxy["alpn"]).join(",");
                    stream->utlsFingerprint = Node2QString(proxy["client-fingerprint"]);
                    if (stream->utlsFingerprint.isEmpty()) {
                        stream->utlsFingerprint = Configs::dataStore->utlsFingerprint;
                    }

                    auto reality = NodeChild(proxy, {"reality-opts"});
                    if (reality.is_mapping()) {
                        stream->reality_pbk = Node2QString(reality["public-key"]);
                        stream->reality_sid = Node2QString(reality["short-id"]);
                    }
                    bean_common.reset();
                } else if (type == "hysteria") {
                    auto bean = ent->unlock(ent->QUICBean());

                    bean->allowInsecure = Node2Bool(proxy["skip-cert-verify"]);
                    auto alpn = Node2QStringList(proxy["alpn"]);
                    bean->caText = Node2QString(proxy["ca-str"]);
                    if (!alpn.isEmpty()) bean->alpn = alpn[0];
                    bean->sni = Node2QString(proxy["sni"]);

                    auto auth_str = FIRST_OR_SECOND(Node2QString(proxy["auth_str"]), Node2QString(proxy["auth-str"]));
                    auto auth = Node2QString(proxy["auth"]);
                    if (!auth_str.isEmpty()) {
                        bean->authPayloadType = Configs::QUICBean::hysteria_auth_string;
                        bean->authPayload = auth_str;
                    }
                    if (!auth.isEmpty()) {
                        bean->authPayloadType = Configs::QUICBean::hysteria_auth_base64;
                        bean->authPayload = auth;
                    }
                    bean->obfsPassword = Node2QString(proxy["obfs"]);

                    if (Node2Bool(proxy["disable_mtu_discovery"]) || Node2Bool(proxy["disable-mtu-discovery"])) bean->disableMtuDiscovery = true;
                    bean->streamReceiveWindow = Node2Int(proxy["recv-window"]);
                    bean->connectionReceiveWindow = Node2Int(proxy["recv-window-conn"]);

                    auto upMbps = Node2QString(proxy["up"]).split(" ")[0].toInt();
                    auto downMbps = Node2QString(proxy["down"]).split(" ")[0].toInt();
                    if (upMbps > 0) bean->uploadMbps = upMbps;
                    if (downMbps > 0) bean->downloadMbps = downMbps;

                    auto ports = Node2QString(proxy["ports"]);
                    if (!ports.isEmpty()) {
                        QStringList serverPorts;
                        ports.replace("/", ",");
                        for (const QString& port : ports.split(",", Qt::SkipEmptyParts)) {
                            if (port.isEmpty()) {
                                continue;
                            }
                            QString modifiedPort = port;
                            modifiedPort.replace("-", ":");
                            serverPorts.append(modifiedPort);
                        }
                        bean->serverPorts = serverPorts;
                    }
                    bean.reset();
                } else if (type == "hysteria2") {
                    auto bean = ent->unlock(ent->QUICBean());

                    bean->allowInsecure = Node2Bool(proxy["skip-cert-verify"]);
                    bean->caText = Node2QString(proxy["ca-str"]);
                    bean->sni = Node2QString(proxy["sni"]);

                    bean->obfsPassword = Node2QString(proxy["obfs-password"]);
                    bean->password = Node2QString(proxy["password"]);

                    bean->uploadMbps = Node2QString(proxy["up"]).split(" ")[0].toInt();
                    bean->downloadMbps = Node2QString(proxy["down"]).split(" ")[0].toInt();

                    auto ports = Node2QString(proxy["ports"]);
                    if (!ports.isEmpty()) {
                        QStringList serverPorts;
                        ports.replace("/", ",");
                        for (const QString& port : ports.split(",", Qt::SkipEmptyParts)) {
                            if (port.isEmpty()) {
                                continue;
                            }
                            QString modifiedPort = port;
                            modifiedPort.replace("-", ":");
                            serverPorts.append(modifiedPort);
                        }
                        bean->serverPorts = serverPorts;
                    }
                    bean.reset();
                } else if (type == "tuic") {
                    auto bean = ent->unlock(ent->QUICBean());

                    bean->uuid = Node2QString(proxy["uuid"]);
                    bean->password = Node2QString(proxy["password"]);

                    if (Node2Int(proxy["heartbeat-interval"]) != 0) {
                        bean->heartbeat = QString::number(Node2Int(proxy["heartbeat-interval"])) + "ms";
                    }

                    bean->udpRelayMode = Node2QString(proxy["udp-relay-mode"], bean->udpRelayMode);
                    bean->congestionControl = Node2QString(proxy["congestion-controller"], bean->congestionControl);

                    bean->disableSni = Node2Bool(proxy["disable-sni"]);
                    bean->zeroRttHandshake = Node2Bool(proxy["reduce-rtt"]);
                    bean->allowInsecure = Node2Bool(proxy["skip-cert-verify"]);
                    bean->alpn = Node2QStringList(proxy["alpn"]).join(",");
                    bean->caText = Node2QString(proxy["ca-str"]);
                    bean->sni = Node2QString(proxy["sni"]);

                    if (Node2Bool(proxy["udp-over-stream"])) bean->uos = true;

                    if (!Node2QString(proxy["ip"]).isEmpty()) {
                        if (bean->sni.isEmpty()) bean->sni = bean->entity->serverAddress;
                        bean->entity->serverAddress = Node2QString(proxy["ip"]);
                    }
                    bean.reset();
                } else {
                    continue;
                }

                if (needFix) RawUpdater_FixEnt(ent);
                updated_order += ent;
            }
        } catch (const fkyaml::exception &ex) {
   //         runOnUiThread([=,this] {
                qDebug() << ("YAML Exception") <<  ex.what();
     //       });
        }
    }

    //
    void GroupUpdater::AsyncUpdate(
            const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob, 
            const QString &str, const std::function<QString(bool*,bool*,const QString&)> &info, 
            int _sub_gid, const std::function<void()> &finish) {
        auto content = str.trimmed();
        bool asURL = false;
        bool createNewGroup = false;
        QString groupName = "";

        if (_sub_gid < 0 && (content.startsWith("http://") || content.startsWith("https://"))) {
            bool ok = false;
            groupName = info(&ok, &createNewGroup, content);
            asURL = true;
            if (ok == false){
                return;
            }
            if (!createNewGroup && groupName == "link"){
                asURL = false;
            }
        }

        runOnNewThread([=,this] {
            auto gid = _sub_gid;
            if (createNewGroup) {
                auto group = Configs::ProfileManager::NewGroup();
                if (groupName == "") {
                    group->name = QUrl(str).host();
                } else {
                    group->name = groupName;
                }
                group->url = str;
                Configs::profileManager->AddGroup(group);
                gid = group->id;
                MW_dialog_message("SubUpdater", "NewGroup");
            }
            Update(PreFinishJob, str, gid, asURL);
            emit asyncUpdateCallback(gid);
            if (finish != nullptr) finish();
        });
    }

    /*

    */

    void GroupUpdater::Update(const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob, 
            const QString &_str, int _sub_gid, bool _not_sub_as_url) {
        //
        Configs::dataStore->imported_count = 0;
        auto rawUpdater = std::make_unique<RawUpdater>();
        rawUpdater->gid_add_to = _sub_gid;

        //
        QString sub_user_info;
        bool asURL = _sub_gid >= 0 || _not_sub_as_url;
        auto content = _str.trimmed();
        auto group = Configs::profileManager->GetGroup(_sub_gid);
        if (group != nullptr && group->archive) return;

        //
        if (asURL) {
            auto groupName = group == nullptr ? content : group->name;
            MW_show_log(">>>>>>>> " + QObject::tr("Requesting subscription: %1").arg(groupName));

            auto resp = NetworkRequestHelper::HttpGet(content, Configs::dataStore->sub_send_hwid);
            if (!resp.error.isEmpty()) {
                MW_show_log("<<<<<<<< " + QObject::tr("Requesting subscription %1 error: %2").arg(groupName, resp.error + "\n" + resp.data));
                return;
            }

            content = resp.data;
            sub_user_info = NetworkRequestHelper::GetHeader(resp.header, "Subscription-UserInfo");

            MW_show_log("<<<<<<<< " + QObject::tr("Subscription request fininshed: %1").arg(groupName));
        }

        QList<std::shared_ptr<Configs::ProxyEntity>> in;          //
        QList<std::shared_ptr<Configs::ProxyEntity>> out_all;     //
        QList<std::shared_ptr<Configs::ProxyEntity>> out;         //
        QList<std::shared_ptr<Configs::ProxyEntity>> only_in;     //
        QList<std::shared_ptr<Configs::ProxyEntity>> only_out;    //
        QList<std::shared_ptr<Configs::ProxyEntity>> update_del;  //
        QList<std::shared_ptr<Configs::ProxyEntity>> update_keep; //

        if (group != nullptr) {
            in = group->GetProfileEnts();
            group->sub_last_update = QDateTime::currentMSecsSinceEpoch() / 1000;
            group->info = sub_user_info;
            group->Save();
            //
            if (Configs::dataStore->sub_clear) {
                MW_show_log(QObject::tr("Clearing servers..."));
                Configs::profileManager->BatchDeleteProfiles(group->profiles);
            }
        }

        MW_show_log(">>>>>>>> " + QObject::tr("Processing subscription data..."));
        rawUpdater->update(content);
        Configs::profileManager->AddProfileBatch(rawUpdater->updated_order, rawUpdater->gid_add_to);
        MW_show_log(">>>>>>>> " + QObject::tr("Process complete, applying..."));

        if (group != nullptr) {
            out_all = group->GetProfileEnts();

            QString change_text;

            if (Configs::dataStore->sub_clear) {
                // all is new profile
                for (const auto &ent: out_all) {
                    change_text += "[+] " + ent->DisplayTypeAndName() + "\n";
                }
            } else {
                // find and delete not updated profile by ProfileFilter
                Configs::ProfileFilter::OnlyInSrc_ByPointer(out_all, in, out);
                Configs::ProfileFilter::OnlyInSrc(in, out, only_in);
                Configs::ProfileFilter::OnlyInSrc(out, in, only_out);
                Configs::ProfileFilter::Common(in, out, update_keep, update_del, false);
                QString notice_added;
                QString notice_deleted;
                if (only_out.size() < 1000)
                {
                    for (const auto &ent: only_out) {
                        notice_added += "[+] " + ent->DisplayTypeAndName() + "\n";
                    }
                } else
                {
                    notice_added += QString("[+] ") + "added " + QString::number(only_out.size()) + "\n";
                }
                if (only_in.size() < 1000)
                {
                    for (const auto &ent: only_in) {
                        notice_deleted += "[-] " + ent->DisplayTypeAndName() + "\n";
                    }
                } else
                {
                    notice_deleted += QString("[-] ") + "deleted " + QString::number(only_in.size()) + "\n";
                }


                // sort according to order in remote
                group->profiles.clear();
                for (const auto &ent: rawUpdater->updated_order) {
                    auto deleted_index = update_del.indexOf(ent);
                    if (deleted_index >= 0) {
                        if (deleted_index >= update_keep.count()) continue; // should not happen
                        const auto& ent2 = update_keep[deleted_index];
                        group->profiles.append(ent2->id);
                    } else {
                        group->profiles.append(ent->id);
                    }
                }
                group->Save();

                // cleanup
                QList<int> del_ids;
                for (const auto &ent: out_all) {
                    if (!group->HasProfile(ent->id)) {
                        del_ids.append(ent->id);
                    }
                }
                Configs::profileManager->BatchDeleteProfiles(del_ids);

                change_text = "\n" + QObject::tr("Added %1 profiles:\n%2\nDeleted %3 Profiles:\n%4")
                                         .arg(only_out.length())
                                         .arg(notice_added)
                                         .arg(only_in.length())
                                         .arg(notice_deleted);
                if (only_out.length() + only_in.length() == 0) change_text = QObject::tr("Nothing");
            }

            MW_show_log("<<<<<<<< " + QObject::tr("Change of %1:").arg(group->name) + "\n" + change_text);
            PreFinishJob(group);

            MW_dialog_message("SubUpdater", "finish-dingyue");
        } else {
            Configs::dataStore->imported_count = rawUpdater->updated_order.count();
            MW_dialog_message("SubUpdater", "finish");
        }
    }
} // namespace Subscription

bool UI_update_all_groups_Updating = false;

#define should_skip_group(g) (g == nullptr || g->url.isEmpty() || g->archive || (onlyAllowed && g->skip_auto_update))

void serialUpdateSubscription(
        const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob,
        const QList<int> &groupsTabOrder, const std::function<QString(bool*,bool*,const QString&)> &info, 
        int _order, bool onlyAllowed) {
    if (_order >= groupsTabOrder.size()) {
        UI_update_all_groups_Updating = false;
        return;
    }

    // calculate this group
    auto group = Configs::profileManager->GetGroup(groupsTabOrder[_order]);
    if (group == nullptr || should_skip_group(group)) {
        serialUpdateSubscription(PreFinishJob, groupsTabOrder, info, _order + 1, onlyAllowed);
        return;
    }

    int nextOrder = _order + 1;
    while (nextOrder < groupsTabOrder.size()) {
        auto nextGid = groupsTabOrder[nextOrder];
        auto nextGroup = Configs::profileManager->GetGroup(nextGid);
        if (!should_skip_group(nextGroup)) {
            break;
        }
        nextOrder += 1;
    }

    // Async update current group
    UI_update_all_groups_Updating = true;
    Subscription::groupUpdater->AsyncUpdate(PreFinishJob, group->url, info, group->id, [=] {
        serialUpdateSubscription(PreFinishJob, groupsTabOrder, info, nextOrder, onlyAllowed);
    });
}

void UI_update_all_groups(
        const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob,
        bool onlyAllowed, const std::function<QString(bool*, bool*, const QString&)> &info) {
    if (UI_update_all_groups_Updating) {
        MW_show_log("The last subscription update has not exited.");
        return;
    }

    auto groupsTabOrder = Configs::profileManager->groupsTabOrder;
    serialUpdateSubscription(PreFinishJob, groupsTabOrder, info, 0, onlyAllowed);
}
