# NekoBox for PC
Qt based Desktop cross-platform GUI proxy utility, empowered by [Sing-box](https://github.com/SagerNet/sing-box)

Supports Windows 11/10/8/7 / Linux / MacOS out of the box.

### GitHub Releases (Portable ZIP)

[![GitHub All Releases](https://img.shields.io/github/downloads/qr243vbi/nekobox/total?label=downloads-total&logo=github&style=flat-square)](https://github.com/qr243vbi/nekobox/releases)

### OBS repository
[NekoBox repository](https://software.opensuse.org//download.html?project=home%3Ajuzbun%3ANekoBox&package=nekobox) for various linux distributions.

## Supported protocols

- SOCKS
- HTTP(S)
- Shadowsocks
- Trojan
- VMess
- VLESS
- TUIC
- Hysteria
- Hysteria2
- AnyTLS
- Wireguard
- SSH
- Custom Outbound
- Custom Config
- Chaining outbounds
- Extra Core

## Subscription Formats

Various formats are supported, including share links, JSON array of outbounds and v2rayN link format as well as limited support for Shadowsocks and Clash formats.

## Credits

- [SagerNet/sing-box](https://github.com/SagerNet/sing-box)
- [Qv2ray](https://github.com/Qv2ray/Qv2ray)
- [Qt](https://www.qt.io/)
- [protorpc](https://github.com/chai2010/protorpc)
- [fkYAML](https://github.com/fktn-k/fkYAML)
- [quirc](https://github.com/dlbeer/quirc)
- [QHotkey](https://github.com/Skycoder42/QHotkey)

## FAQ
**Why does my Anti-Virus detect NekoBox as malware?** <br/>
NekoBox's built-in update functionallity downloads the new release, removes the old files and replaces them with the new ones, which is quite simliar to what malwares do, remove your files and replace them with an encrypted version of your files.
Also the `System DNS` feature will change your system's DNS settings, which is also considered a dangerous action by some Anti-Virus applications.
