# NekoRay

Qt based Desktop cross-platform GUI proxy configuration manager (backend: sing-box)

Supports Windows / Linux / MacOS out of the box now. Windows7 is also supported, but requires some additional changes to run.

### How to run on Windows7
To run on Windows7, you will need to utilize [VxKex](https://github.com/vxiiduu/VxKex/). Some of Tun mode stacks might not work as well, please fallback to gVisor stack if you encountered this problem.

### Compatiblity issues on Linux Distros
Since every Distro of Linux has its own set of libraries, you might need to manually install some packages in order for nekoray to run.
Currently known libraries that might be missing: `libxcb-cursor`. Please make sure to install them if nekoray did not start.

### Using Tun mode in MacOS
To use Tun mode in MacOS, you need to open the app as root.

Open the app like this:

```shell
sudo /Applications/nekoray.app/Contents/MacOS/nekoray
```

It will open a Nekoray instance with no configurations; if you have configured your Nekoray in normal mode, close the app and do this to copy them:

**`Note:`** Replace `YOU` with your username

```shell
sudo cp -r /Users/YOU/Library/Preferences/nekoray/ /private/var/root/Library/Preferences/nekoray
```

Now, you can open it with the first command.


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

We support widely used link formats(like Shadowsocks, Clash and v2rayN) as well as custom
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
