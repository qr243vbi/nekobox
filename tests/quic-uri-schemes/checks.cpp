int checked = 0, failed = 0;
void check(bool ok, const QString &label) {
 ++checked;
 if (!ok) { ++failed; if (failed <= 50) std::cout << "FAIL " << label.toStdString() << '\n'; }
}
QList<std::shared_ptr<Configs::ProxyEntity>> imported(const QString &input) {
 Subscription::RawUpdater raw; raw.update(input); return raw.proxies;
}
QString uri(const QString &scheme, const QString &host, const QString &port, const QString &suffix = "/?sni=sni.invalid#synthetic") {
 return scheme + "://synthetic-user:synthetic-pass@" + host + port + suffix;
}
void reject(const QString &input, const QString &label) {
 auto entities = imported(input); check(entities.empty(), label + " rejected");
}
void retained(const QString &input, const QString &host, int port, const QStringList &ranges, const QString &scheme, const QString &label) {
 auto entities = imported(input); check(entities.size() == 1, label + " imported once"); if (entities.size() != 1) return;
 auto entity = entities[0]; auto bean = std::dynamic_pointer_cast<const Configs::QUICBean>(entity->bean());
 check(bean != nullptr, label + " QUIC bean"); if (!bean) return;
 check(entity->isValid(), label + " recognized entity type");
 check(entity->type == scheme, label + " correct protocol");
 check(entity->serverAddress == host, label + " unchanged host");
 if (port != -999) check(entity->serverPort == port, label + " expected scalar port");
 check(bean->serverPorts == ranges, label + " exact port union");
 auto result = bean->BuildCoreObjSingBox(); auto config = result.outbound;
 check(result.error.isEmpty(), label + " config builder success");
 check(config["type"].toString() == scheme && config["server"].toString() == host, label + " core destination");
 if (ranges.isEmpty()) check(config.contains("server_port") && config["server_port"].toInt() == entity->serverPort && !config.contains("server_ports"), label + " scalar config");
 else {
  QJsonArray expected;
  for(const auto &range : ranges) expected.append(range.contains(':') ? range : range + ":" + range);
  check(!config.contains("server_port") && config["server_ports"].toArray() == expected, label + " exact core ranges");
 }
 if (scheme == "hysteria2") check(bean->password == "synthetic-user:synthetic-pass", label + " userpass preserved");
 if (scheme == "tuic") check(bean->uuid == "synthetic-user" && bean->password == "synthetic-pass", label + " TUIC credentials preserved");
}
void controls() {
 for (const QString scheme : {"hy2", "hysteria2", "HY2", "HYSTERIA2", "tuic", "TUIC", "hysteria", "HYSTERIA"}) {
  const auto type = scheme.toLower().contains("hysteria") && !scheme.contains('2') ? "hysteria" : (scheme.toLower() == "tuic" ? "tuic" : "hysteria2");
  for (const QString host : {"example.invalid", "192.0.2.1", "[2001:db8::1]"}) {
   const auto unbracketed = host.startsWith('[') ? host.mid(1, host.size()-2) : host;
   for (const QString port : {"0", "1", "443", "65535"}) {
    retained(uri(scheme, host, ":" + port, "/?upmbps=10&downmbps=10&sni=sni.invalid#synthetic"), unbracketed, port.toInt(), {}, type, scheme + " " + host + ":" + port);
   }
  }
 }
 // Omitted-port semantics are deliberately not asserted here; PR323 owns 443.
 for (const QString scheme : {"hy2", "hysteria2", "tuic"}) retained(uri(scheme,"example.invalid",""),"example.invalid",-999,{},scheme == "tuic" ? "tuic" : "hysteria2",scheme + " omitted control");
 for (const QString scheme : {"hysteria", "hy2", "hysteria2"}) {
  retained(uri(scheme,"example.invalid",":8443","?upmbps=10&downmbps=10&mport=443,8443-8445&hop_interval=10s"), "example.invalid", 8443, {"443","8443:8445"}, scheme == "hysteria" ? "hysteria" : "hysteria2", scheme + " legacy mport");
 }
}
void aliases() {
 for (const QString scheme : {"hysteria-2", "HYSTERIA-2", "h-y-2", "hysteria--2", "tu-ic", "hyste-ria", "hy"}) {
  for(const QString port : {":8443", ""}) {
   const auto input = uri(scheme,"example.invalid",port);
   reject(input,scheme + port);
   reject(QString::fromLatin1(input.toUtf8().toBase64()), scheme + port + " base64");
   reject(QString::fromUtf8(QJsonDocument(QJsonArray{input}).toJson(QJsonDocument::Compact)), scheme + port + " JSON string array");
  }
 }
 for(const QString scheme : {"https", "unknown", "hysteria-2"}) {
  Configs::ProxyEntity entity("hysteria2"); Configs::QUICBean bean(&entity, Configs::QUICBean::proxy_Hysteria2);
  check(!bean.TryParseLink(uri(scheme,"example.invalid",":8443")), scheme + " direct parser must reject unsupported scheme");
 }
 auto mixed=imported(uri("hysteria-2","bad.invalid",":443") + "\n" + uri("hy2","good.invalid",":8443"));
 check(mixed.size() == 1 && mixed[0]->serverAddress == "good.invalid", "bad alias does not add phantom profile or drop following good link");
}
int main() {
 Configs::initializeTypes();
 controls();
 aliases();
 std::cout << "RESULT " << checked << " checks, " << failed << " failures\n";
 return failed ? 1 : 0;
}
