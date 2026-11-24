#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QString>

namespace Configs {

// One Xray runtime owns every built-in profile used by this configuration.
// Ports belong to outbound tags, not profile IDs: a profile may occur in
// several chains, and IDs differing by 20000 must not share a listener.
class XrayRuntime {
public:
    QJsonObject config;
    QJsonArray transportInbounds;
    QJsonArray transportRules;

    void reservePort(int port) { if (port > 0) reserved.insert(port); }
    bool empty() const { return profiles.isEmpty(); }
    static QString nativeTag(const QString &tag) { return "neko-xray-" + tag; }

    int portFor(const QString &tag, int preferred) {
        if (ports.contains(tag)) return ports.value(tag);
        constexpr int base = 30000, span = 20000;
        const int start = preferred >= base && preferred < base + span ? preferred : base;
        for (int offset = 0; offset < span; ++offset) {
            const int port = base + (start - base + offset) % span;
            if (!reserved.contains(port)) {
                reserved.insert(port);
                ports.insert(tag, port);
                return port;
            }
        }
        return 0;
    }

    bool addProfile(const QString &tag, const QJsonObject &single) {
        if (profiles.contains(tag)) return true;
        const auto inbounds = single["inbounds"].toArray();
        const auto outbounds = single["outbounds"].toArray();
        if (inbounds.size() != 1 || outbounds.isEmpty() ||
            inbounds.first().toObject()["port"].toInt() != ports.value(tag)) return false;
        auto inbound = inbounds.first().toObject();
        const auto inboundTag = "neko-in-" + tag;
        inbound["tag"] = inboundTag;
        auto outbound = outbounds.first().toObject();
        outbound["tag"] = nativeTag(tag);
        if (config.isEmpty()) {
            config = single;
            config["inbounds"] = QJsonArray{};
            // Retain shared supporting outbounds (e.g. freedom) exactly once.
            auto shared = outbounds;
            shared.removeFirst();
            config["outbounds"] = shared;
        }
        auto allInbounds = config["inbounds"].toArray();
        allInbounds.append(inbound);
        config["inbounds"] = allInbounds;
        auto allOutbounds = config["outbounds"].toArray();
        if (profiles.isEmpty()) allOutbounds.prepend(outbound);
        else allOutbounds.append(outbound);
        config["outbounds"] = allOutbounds;
        auto routing = config["routing"].toObject();
        auto rules = routing["rules"].toArray();
        rules.append(QJsonObject{{"type", "field"}, {"inboundTag", QJsonArray{inboundTag}},
                                 {"outboundTag", nativeTag(tag)}});
        routing["rules"] = rules;
        config["routing"] = routing;
        profiles.insert(tag);
        return true;
    }

    // The local SOCKS bridge must stay local. For an Xray hop, its remote
    // transport (rather than the sing-box -> localhost connection) is chained.
    bool chainThrough(const QString &previous, const QString &next, bool nextIsXray) {
        QString carrier = nativeTag(next);
        if (!nextIsXray) {
            carrier = "neko-transport-" + previous;
            const int port = portFor("transport:" + previous, 0);
            if (port == 0) return false;
            const auto inboundTag = "neko-transport-in-" + previous;
            transportInbounds.append(QJsonObject{{"type", "mixed"}, {"tag", inboundTag},
                {"listen", "127.0.0.1"}, {"listen_port", port}});
            transportRules.append(QJsonObject{{"action", "route"},
                {"inbound", QJsonArray{inboundTag}}, {"outbound", next}});
            auto outbounds = config["outbounds"].toArray();
            outbounds.append(QJsonObject{{"tag", carrier}, {"protocol", "socks"},
                {"settings", QJsonObject{{"servers", QJsonArray{QJsonObject{
                    {"address", "127.0.0.1"}, {"port", port}}}}}}});
            config["outbounds"] = outbounds;
        }
        auto outbounds = config["outbounds"].toArray();
        for (int i = 0; i < outbounds.size(); ++i) {
            auto outbound = outbounds[i].toObject();
            if (outbound["tag"] != nativeTag(previous)) continue;
            auto stream = outbound["streamSettings"].toObject();
            auto sockopt = stream["sockopt"].toObject();
            sockopt["dialerProxy"] = carrier;
            stream["sockopt"] = sockopt;
            outbound["streamSettings"] = stream;
            outbounds[i] = outbound;
            config["outbounds"] = outbounds;
            return true;
        }
        return false;
    }

private:
    QMap<QString, int> ports;
    QSet<int> reserved;
    QSet<QString> profiles;
};
}
