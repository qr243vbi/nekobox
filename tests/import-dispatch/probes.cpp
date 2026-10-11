int main() {
 Configs::initializeTypes();
 for (const QString scheme : {"vless", "hy2", "hysteria2", "HY2", "HYSTERIA2", "hysteria-2", "tuic"}) {
  for (const QString port : {"", ":", ":1", ":443", ":65535", ":0", ":65536", ":port", ":-1", ":443-445", ":443,445"}) {
   QString link=scheme+"://synthetic-auth@example.invalid"+port+"?security=none&type=tcp&packetEncoding=xudp&encryption=none#fixture";
   Subscription::RawUpdater importer;
   importer.update(link);
   std::cout<<"CASE "<<scheme.toStdString()<<" port="<<port.toStdString()<<" count="<<importer.proxies.size();
   if (!importer.proxies.empty()) {
    const auto ent=importer.proxies.front();
    std::cout<<" type="<<ent->type.toStdString()<<" host="<<ent->serverAddress.toStdString()<<" value="<<ent->serverPort<<" display="<<Configs::ProfileManager::GetDisplayType(ent->type).toStdString();
    if(auto quic=std::dynamic_pointer_cast<const Configs::QUICBean>(ent->bean()))std::cout<<" auth="<<quic->password.toStdString()<<" ranges=["<<quic->serverPorts.join(',').toStdString()<<"] size="<<quic->serverPorts.size();
   }
   std::cout<<'\n';
  }
 }
 return 0;
}
