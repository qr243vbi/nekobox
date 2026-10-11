int checked=0, failed=0;
void check(bool ok,const QString &label) {
 ++checked;
 if(!ok){++failed;std::cout<<"FAIL "<<label.toStdString()<<'\n';}
}
std::shared_ptr<Configs::ProxyEntity> imported(const QString &input,const QString &label) {
 Subscription::RawUpdater raw; raw.update(input);
 check(raw.proxies.size()==1,label+" one imported entity");
 return raw.proxies.size()==1 ? raw.proxies.front() : nullptr;
}
void hy2(const QString &uri,const QString &host,int port,const QString &auth,const QString &label) {
 auto ent=imported(uri,label); if(!ent)return;
 check(ent->type=="hysteria2",label+" Hysteria2 type");
 check(Configs::ProfileManager::GetDisplayType(ent->type)=="Hysteria 2",label+" Hysteria2 display");
 check(ent->serverAddress==host,label+" host");
 check(ent->serverPort==port,label+" expected port "+QString::number(port)+", got "+QString::number(ent->serverPort));
 auto bean=std::dynamic_pointer_cast<const Configs::QUICBean>(ent->bean());
 check(bean!=nullptr,label+" QUIC bean"); if(!bean)return;
 check(bean->proxy_type==Configs::QUICBean::proxy_Hysteria2,label+" Hysteria2 bean mode");
 check(bean->password==auth,label+" authentication preserved");
}
int main() {
 Configs::initializeTypes();
 check(Configs::ProxyEntity("hysteria2").serverPort==1080,"fixture uses real production initial port");
 struct Port { const char *suffix; int expected; };
 const Port ports[]={{"",443},{":",443},{":1",1},{":443",443},{":8443",8443},{":65535",65535},{":0",0}};
 for(const QString scheme:{"hy2","hysteria2","HY2","HYSTERIA2"}) {
  for(const auto &port:ports) {
   for(const QString host:{"example.invalid","192.0.2.1","[2001:db8::1]"}) {
    const QString expectedHost=host.startsWith('[') ? host.mid(1,host.size()-2) : host;
    const auto uri=scheme+"://synthetic-auth@"+host+port.suffix+"/?sni=sni.invalid#fixture";
    const auto label=scheme+" "+host+port.suffix;
    hy2(uri,expectedHost,port.expected,"synthetic-auth",label);
   }
  }
  hy2(scheme+"://synthetic-user:synthetic-password@example.invalid/?sni=sni.invalid#fixture","example.invalid",443,"synthetic-user:synthetic-password",scheme+" userpass");
  hy2(scheme+"://example.invalid/?sni=sni.invalid#fixture","example.invalid",443,"",scheme+" optional auth");
  const auto uri=scheme+"://synthetic-auth@example.invalid/?sni=sni.invalid#fixture";
  hy2(QString::fromLatin1(uri.toUtf8().toBase64()),"example.invalid",443,"synthetic-auth",scheme+" base64");
  hy2(QString::fromLatin1(uri.toUtf8().toBase64().toBase64()),"example.invalid",443,"synthetic-auth",scheme+" nested base64");
  hy2(QString::fromUtf8(QJsonDocument(QJsonArray{uri}).toJson(QJsonDocument::Compact)),"example.invalid",443,"synthetic-auth",scheme+" JSON string array");
  auto ent=imported(scheme+"://synthetic-auth@example.invalid?mport=443,8443-8445&hop_interval=10s",scheme+" legacy mport");
  if(ent){auto q=std::dynamic_pointer_cast<const Configs::QUICBean>(ent->bean());check(q && q->serverPorts==QStringList{"443","8443:8445"},scheme+" mport survives");check(ent->serverPort==443,scheme+" mport fallback");}
 }
 for(const QString scheme:{"vless","trojan","tuic"}) {
  for(const auto &port:ports) {
   auto label=scheme+port.suffix;
   auto ent=imported(scheme+"://synthetic-auth@example.invalid"+port.suffix+"?security=none&type=tcp&packetEncoding=xudp&encryption=none#",label);
   if(ent){check(ent->type==scheme,label+" type");check(ent->serverAddress=="example.invalid",label+" host");check(ent->serverPort==port.expected,label+" port unchanged");}
  }
 }
 for(const QString suffix:{":port",":65536",":-1",":443-445",":443,445"}) {
  Subscription::RawUpdater raw;raw.update("vless://synthetic-auth@example.invalid"+suffix+"?security=none&type=tcp&packetEncoding=xudp&encryption=none#");
  check(raw.proxies.empty(),"VLESS malformed port rejected "+suffix);
 }
 for(const QString scheme:{"hysteria","HYSTERIA"}) {
  auto ent=imported(scheme+"://example.invalid:8443?auth=synthetic-auth&upmbps=10&downmbps=10",scheme);
  if(ent){check(ent->type=="hysteria",scheme+" type");check(ent->serverPort==8443,scheme+" explicit port unchanged");}
 }
 Subscription::RawUpdater mixed;
 mixed.update("hy2://synthetic-auth@first.invalid\r\nhysteria2://synthetic-auth@second.invalid:8443\nvless://synthetic-auth@third.invalid:1234");
 check(mixed.proxies.size()==3,"mixed multiline creates three");
 if(mixed.proxies.size()==3){check(mixed.proxies[0]->type=="hysteria2" && mixed.proxies[0]->serverPort==443,"multiline HY2 default");check(mixed.proxies[1]->type=="hysteria2" && mixed.proxies[1]->serverPort==8443,"multiline Hysteria2 explicit");check(mixed.proxies[2]->type=="vless" && mixed.proxies[2]->serverPort==1234,"multiline VLESS");}
 std::cout<<"RESULT "<<checked<<" checks, "<<failed<<" failures\n";
 return failed ? 1 : 0;
}
