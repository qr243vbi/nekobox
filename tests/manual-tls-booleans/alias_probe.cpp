int main() {
    using namespace Configs;
    auto uri = [](const QString &scheme, const QString &query) {
        return scheme + "://synthetic-password@example.invalid:443?security=tls&type=tcp&" + query;
    };
    for (const QString &query : {QString(), QString("insecure=1"), QString("insecure=true"),
             QString("allowInsecure=0"), QString("allowInsecure=false"),
             QString("insecure=1&allowInsecure=0"), QString("insecure=0&allowInsecure=1"),
             QString("insecure=1&allowInsecure="), QString("insecure=&allowInsecure=1")}) {
        for (int type : {TrojanVLESSBean::proxy_Trojan, TrojanVLESSBean::proxy_VLESS}) {
            ProxyEntity entity;
            TrojanVLESSBean bean(&entity, type);
            const bool parsed = bean.TryParseLink(uri(bean.type(), query));
            QJsonObject observation{{"scheme", bean.type()}, {"query", query}, {"parsed", parsed},
                                    {"field", bean.stream->allow_insecure}, {"outbound", bean.BuildCoreObjSingBox().outbound}};
            std::cout << QJsonDocument(observation).toJson(QJsonDocument::Compact).toStdString() << '\n';
        }
    }
    for (int type : {TrojanVLESSBean::proxy_Trojan, TrojanVLESSBean::proxy_VLESS}) {
        for (bool value : {false, true}) {
            ProxyEntity original_entity, imported_entity;
            TrojanVLESSBean original(&original_entity, type), imported(&imported_entity, type);
            original.TryParseLink(uri(original.type(), value ? "allowInsecure=1" : ""));
            const QString link = original.ToShareLink();
            const bool parsed = imported.TryParseLink(link);
            QJsonObject observation{{"scheme", original.type()}, {"roundtrip_requested", value},
                                    {"exported", link}, {"parsed", parsed},
                                    {"reimported", imported.stream->allow_insecure},
                                    {"outbound", imported.BuildCoreObjSingBox().outbound}};
            std::cout << QJsonDocument(observation).toJson(QJsonDocument::Compact).toStdString() << '\n';
        }
    }
    for (bool value : {false, true}) {
        ProxyEntity original_entity, imported_entity;
        AnyTLSBean original(&original_entity), imported(&imported_entity);
        original.TryParseLink(uri("anytls", value ? "insecure=1" : ""));
        const QString link = original.ToShareLink();
        imported.TryParseLink(link);
        QJsonObject observation{{"scheme", "anytls"}, {"roundtrip_requested", value}, {"exported", link},
                                {"reimported", imported.stream->allow_insecure}};
        std::cout << QJsonDocument(observation).toJson(QJsonDocument::Compact).toStdString() << '\n';
    }
}
