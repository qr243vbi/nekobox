#pragma once
#include <QStringList>
#include <QMap>

namespace Preset::SingBox {
  inline QMap<QString, QString> OutboundTypes = {
      {"socks", "Socks"},
      {"mieru", "Mieru"},
      {"http", "HTTP"},
      {"shadowsocks", "Shadowsocks"},
      {"chain", "Chain Proxy"},
      {"vmess", "VMess"},
      {"trojan", "Trojan"},
      {"vless", "VLESS"},
      {"hysteria", "Hysteria 1"},
      {"hysteria2", "Hysteria 2"},
      {"tuic", "TUIC"},
      {"anytls", "AnyTLS"},
      {"shadowtls", "ShadowTLS"},
      {"wireguard", "Wireguard"},
      {"tailscale", "Tailscale"},
      {"ssh", "SSH"},
      {"custom", "Custom"},
      {"extracore", "Extra Core"},
      {"tor", "Tor"}
  };
  inline QStringList VpnImplementation = {"system", "gvisor", "mixed"};
  inline QStringList MieruMultiplexing = {
      "MULTIPLEXING_OFF",
      "MULTIPLEXING_LOW",
      "MULTIPLEXING_MIDDLE",
      "MULTIPLEXING_HIGH"
  };
  inline QStringList MieruTransport = {
      "TCP",
      "UDP"
  };
  inline QStringList DomainStrategy = {"", "ipv4_only", "ipv6_only", "prefer_ipv4", "prefer_ipv6"};
  inline QStringList UtlsFingerPrint = {"", "chrome", "firefox", "edge", "safari", "360", "qq", "ios", "android", "random", "randomized"};
  inline QStringList ShadowsocksMethods = {
    "2022-blake3-aes-128-gcm", 
    "2022-blake3-aes-256-gcm", 
    "2022-blake3-chacha20-poly1305", 
    "none", "aes-128-gcm", 
    "aes-192-gcm", 
    "aes-256-gcm", 
    "chacha20-ietf-poly1305", 
    "xchacha20-ietf-poly1305", 
    "aes-128-ctr", 
    "aes-192-ctr", 
    "aes-256-ctr", 
    "aes-128-cfb", 
    "aes-192-cfb", 
    "aes-256-cfb", 
    "rc4-md5", 
    "chacha20-ietf", 
    "xchacha20"
  };
  inline QStringList V2RAYTransports = {"http", "grpc", "quic", "httpupgrade", "ws", "tcp", "xhttp", "kcp"};
  inline QStringList Flows = {"xtls-rprx-vision"};
  inline QStringList SniffProtocols = {"http", "tls", "quic", "stun", "dns", "bittorrent", "dtls", "ssh", "rdp"};
  inline QStringList ActionTypes = {"route", "reject", "hijack-dns", "route-options", "sniff", "resolve"};
  inline QStringList rejectMethods = {"default", "drop"};
} // namespace Preset::SingBox
