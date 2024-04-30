# NekoRay / NekoBox For PC

Qt based cross-platform GUI proxy configuration manager (backend: sing-box)

Support Windows / Linux out of the box now.

Since the original author has abandoned the project, I have picked it up and will try to maintain it. Currently Sing-box has been updated to the latest version, Support for Xray has been removed, Mux and UoT settings will be imported/exported and also hysteria port hopping support has been removed. New features will hopefully be eventually added, but since I am not familiar with the Qt framework it might take some time. PRs are also welcome to add new features or improve the code strcuture. At the point my main focus will be to further simplify the structure of the project and make it more concise and easier to fork and build.
### GitHub Releases (Portable ZIP)

[![GitHub All Releases](https://img.shields.io/github/downloads/Mahdi-zarei/nekoray/total?label=downloads-total&logo=github&style=flat-square)](https://github.com/Mahdi-zarei/nekoray/releases)

## 代理 / Proxy

- SOCKS (4/4a/5)
- HTTP(S)
- Shadowsocks
- VMess
- VLESS
- Trojan
- TUIC ( sing-box )
- NaïveProxy ( Custom Core )
- Hysteria ( Custom Core or sing-box )
- Hysteria2 ( Custom Core or sing-box )
- Custom Outbound
- Custom Config
- Custom Core

## 订阅 / Subscription

- Raw: some widely used formats (like Shadowsocks, Clash and v2rayN)
- 原始格式: 一些广泛使用的格式 (如 Shadowsocks、Clash 和 v2rayN)

## Credits

Core:

- [SagerNet/sing-box](https://github.com/SagerNet/sing-box)
- [Matsuridayo/sing-box-extra](https://github.com/Mahdi-zarei/sing-box-extra)

Gui:

- [Qv2ray](https://github.com/Qv2ray/Qv2ray)
- [Qt](https://www.qt.io/)
- [protobuf](https://github.com/protocolbuffers/protobuf)
- [yaml-cpp](https://github.com/jbeder/yaml-cpp)
- [zxing-cpp](https://github.com/nu-book/zxing-cpp)
- [QHotkey](https://github.com/Skycoder42/QHotkey)
- [AppImageKit](https://github.com/AppImage/AppImageKit)
