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
void roundtrips() {
 for (const QString scheme : {"hy2", "hysteria2", "hysteria"}) {
  for (const QString host : {"example.invalid", "[2001:db8::1]"}) {
   for (const QString range : {"443-445", "443,8443", "443,8443-8445"}) {
    const auto input = uri(scheme,host,":"+range,"?sni=sni.invalid&upmbps=10&downmbps=10&hop_interval=10s#synthetic");
    auto entities = imported(input); check(entities.size()==1, "roundtrip original imported"); if(entities.size()!=1) continue;
    auto bean=std::dynamic_pointer_cast<const Configs::QUICBean>(entities[0]->bean());
    const auto exported=bean->ToShareLink();
    check(exported.contains(":"+range+"?"), "export retains authority union");
    auto reparsed=imported(exported); check(reparsed.size()==1, "export reimported"); if(reparsed.size()!=1) continue;
    auto other=std::dynamic_pointer_cast<const Configs::QUICBean>(reparsed[0]->bean());
    check(bean->serverPorts==other->serverPorts, "native range/list roundtrip");
    check(bean->BuildCoreObjSingBox().outbound==other->BuildCoreObjSingBox().outbound, "native roundtrip exact generated config");
    check(entities[0]->name==reparsed[0]->name, "native roundtrip name");
   }
  }
 }
}
void ports() {
 struct Case { QString raw; QStringList normalized; };
 const QList<Case> valid = {{"0-65535",{"0:65535"}}, {"0,443",{"0","443"}}, {"1-65535",{"1:65535"}}, {"443-445",{"443:445"}}, {"443,8443",{"443","8443"}}, {"443,8443-8445,65535",{"443","8443:8445","65535"}}, {"443-443",{"443:443"}}, {"0443,08443-08445",{"0443","08443:08445"}}};
 for(const QString scheme : {"hy2","hysteria2","HY2","HYSTERIA2","hysteria","HYSTERIA"}) {
  const auto type = scheme.toLower() == "hysteria" ? "hysteria" : "hysteria2";
  for(const QString host : {"example.invalid","192.0.2.1","[2001:db8::1]"}) {
   const auto unbracketed = host.startsWith('[') ? host.mid(1, host.size()-2) : host;
   for(const auto &c : valid) {
    for(const QString suffix : {"?upmbps=10&downmbps=10&sni=sni.invalid#synthetic", "/?upmbps=10&downmbps=10#synthetic"}) {
     const auto input = uri(scheme,host,":"+c.raw,suffix);
     retained(input,unbracketed,0,c.normalized,type,scheme+" "+host+":"+c.raw+suffix);
    }
   }
  }
 }
 const QStringList invalid = {"port","65536","-1","+443","443x","443,","443,,445",",443","443-","-445","445-443","1-65536","65536-65537","443-445-447","443,port","443,%34%34%35","443%2C445","443:445","443 -445","443- 445","99999999999999999999999999999"};
 for(const QString scheme : {"hy2","hysteria2","tuic","hysteria"}) {
  for(const QString host : {"example.invalid","[2001:db8::1]"}) {
   for(const auto &port : invalid) reject(uri(scheme,host,":"+port,"?upmbps=10&downmbps=10"),scheme+" "+host+":"+port);
  }
 }
 for(const QString port : {"443-445","443,8443"}) reject(uri("tuic","example.invalid",":"+port),"TUIC does not support ranges "+port);
 for(const QString host : {"bad[host.invalid","[bad::host]","example.invalid:abc",""}) reject(uri("hy2",host,":443-445"),"invalid host not repaired by range parsing "+host);
 for(const QString suffix : {"", "/", "#synthetic", "?sni=sni.invalid", "/?sni=sni.invalid#name:99,100", "/path:99,100?x=other.invalid:7-9"}) retained(uri("hy2","example.invalid",":443-445",suffix),"example.invalid",0,{"443:445"},"hysteria2","authority extraction "+suffix);
 const auto input=uri("hy2","example.invalid",":443,8443-8445");
 retained(QString::fromLatin1(input.toUtf8().toBase64()),"example.invalid",0,{"443","8443:8445"},"hysteria2","range base64");
 retained(QString::fromUtf8(QJsonDocument(QJsonArray{input}).toJson(QJsonDocument::Compact)),"example.invalid",0,{"443","8443:8445"},"hysteria2","range JSON string array");
 auto mixed=imported(uri("hy2","bad.invalid",":65536") + "\n" + uri("hy2","good.invalid",":443-445"));
 check(mixed.size() == 1 && mixed[0]->serverAddress == "good.invalid", "invalid port does not add phantom profile or drop following good range");
}
int main() {
 Configs::initializeTypes();
 controls();
 ports(); roundtrips();
 std::cout << "RESULT " << checked << " checks, " << failed << " failures\n";
 return failed ? 1 : 0;
}
