# NekoRay

Qt based Desktop cross-platform GUI proxy configuration manager (backend: sing-box)

Supports Windows / Linux / MacOS out of the box now. Windows7 is also supported, but requires some additional changes to run.

### Notes on the new versions on Windows
The newest versions of Nekoray are built using the latest compilers and Qt releases, and so they need the latest `vc_redist` installed. If the app does not start and crashes, please make sure to update it.
Also few anti-virus apps may identify the `nekobox_core.exe` as trojan, this is caused by the newely added code which can be used to alter the system DNS settings to hijack all dns requests to Nekoray to route them. Some apps may also 
report `updater.exe` as trojan, this is also a false alarm caused by updater having to delete the old Nekoray files and replacing it with the new ones, which is like what many viruses do to replace your files with encrypted ones for ransoming purposes.

### How to run on Windows7
Maintaining a windows7 build is no longer feasable, use [nekoray-win7](https://github.com/parhelia512/nekoray-win7) for windows7.

### Compatiblity issues on Linux Distros
Since every Distro of Linux has its own set of libraries, you might need to manually install some packages in order for nekoray to run.
Currently known libraries that might be missing: `libxcb-cursor`. Please make sure to install them if nekoray did not start.

### Using Tun mode in MacOS after 4.2.6
To use Tun mode on MacOS, you will need to run `nekobox_core` as admin. Due to restrictive security of MacOS, all downloded apps will be quarantined which makes their content read-only. To fix this, after extracting the contents, you need to run `xattr -d com.apple.quarantine /path/to/nekoray.app` and then open the app and proceed to upgarde to admin by using Tun mode.


### GitHub Releases (Portable ZIP)

[![GitHub All Releases](https://img.shields.io/github/downloads/Mahdi-zarei/nekoray/total?label=downloads-total&logo=github&style=flat-square)](https://github.com/Mahdi-zarei/nekoray/releases)

## Proxy

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

## Subscription

We support widely used link formats(like Shadowsocks and v2rayN) as well as custom
outbound and custom configs. The subscription file can contain commented lines(starting with // or #, empty lines are ignored as well),
JSON strings(can be human-readable or compact) and import links(each link should be on a separate line).

## Credits

Core:

- [SagerNet/sing-box](https://github.com/SagerNet/sing-box)

Gui:

- [Qv2ray](https://github.com/Qv2ray/Qv2ray)
- [Qt](https://www.qt.io/)
- [protobuf](https://github.com/protocolbuffers/protobuf)
- [yaml-cpp](https://github.com/jbeder/yaml-cpp)
- [zxing-cpp](https://github.com/nu-book/zxing-cpp)
- [QHotkey](https://github.com/Skycoder42/QHotkey)
