# NekoBox for PC
Qt based Desktop cross-platform GUI proxy utility, empowered by [Sing-box](https://github.com/SagerNet/sing-box)

Supports Windows 11/10/8/7 (arm64, i386, x86_64) / Linux / MacOS (arm64, x86_64) out of the box.
 
### GitHub Releases (Portable ZIPs, Windows installers)

[![GitHub All Releases](https://img.shields.io/github/downloads/qr243vbi/nekobox/total?label=downloads-total&logo=github&style=flat-square)](https://github.com/qr243vbi/nekobox/releases)

### OBS repository
[NekoBox repository](https://software.opensuse.org//download.html?project=home%3Ajuzbun%3ANekoBox&package=nekobox) for various linux distributions ([OpenSUSE](https://get.opensuse.org/), [Fedora](https://fedoraproject.org/), [Mageia](https://www.mageia.org/), [Debian](https://www.debian.org/), [Raspbian](https://www.raspberrypi.com/software/), [Ubuntu](https://ubuntu.com/), [Arch](https://archlinux.org/)).

### COPR repository
[NekoBox repository](https://copr.fedorainfracloud.org/coprs/qr243vbi/NekoBox/) for various linux distributions ([RedHat](https://www.redhat.com), [Centos](https://www.centos.org), [OpenSUSE](https://get.opensuse.org/), [Fedora](https://fedoraproject.org/), [Mageia](https://www.mageia.org/), [Almalinux](https://almalinux.org/)).

### Chocolatey Package (not approved yet)

[![Chocolatey Package For Windows](https://img.shields.io/chocolatey/dt/nekobox?style=flat-square&logo=chocolatey&label=downloads-total
)](https://community.chocolatey.org/packages/nekobox)

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
- [simple-protobuf](https://github.com/tonda-kriz/simple-protobuf)
- [quickjs](https://quickjs-ng.github.io/quickjs)
- [qrcodegen](https://www.nayuki.io/page/qr-code-generator-library)
- [Setup MinGW Github Action](https://github.com/bwoodsend/setup-winlibs-action)
- [Setup MSVC Github Action](https://github.com/ilammy/msvc-dev-cmd)
- [Setup Ninja Github Action](https://github.com/seanmiddleditch/gha-setup-ninja)
- [Cache Apt Pkgs Github Action](https://github.com/awalsh128/cache-apt-pkgs-action)
- [Setup Qt Github Action](https://github.com/jurplel/install-qt-action)
- [linuxdeploy](https://github.com/linuxdeploy/linuxdeploy)
- [throneproj](https://github.com/throneproj/Throne)
- [MinGW](https://www.mingw-w64.org)
- [MSVC](https://visualstudio.microsoft.com/)
- [go](https://go.dev/)
- [nekoray](https://github.com/MatsuriDayo/nekoray)
- [Open Build Service](https://openbuildservice.org/)
- [Github](https://github.com)
- [cv2pdb](https://github.com/rainers/cv2pdb)
- [cmake](https://gitlab.kitware.com/cmake/cmake)
- [ninja-build](https://ninja-build.org/)
- [codeclysm/extract](https://github.com/codeclysm/extract)
- [shlex](https://github.com/google/shlex)
- [URLParser](https://github.com/dongbum/URLParser)
- [npipe](https://github.com/NullYing/npipe)
- [Fedora COPR](https://copr.fedorainfracloud.org/)
- [Chocolatey Software](https://chocolatey.org/)

## TODO
- Command line tools
- [OpenRC](https://openrc.run/)/[runit](https://smarden.org/runit/)/[systemd](https://systemd.io/) integration of nekobox_core
- Terminal UI
- Remote control
- Support for BSD, Solaris, Android platforms

## FAQ
**What is [Throne](https://throne.com/landing)?** <br/>
About Throne

Throne powers the infrastructure that creators need to better engage and monetize their audiences. Over 1,000,000 creators from 89 countries use Throne's all-in-one wishlist builder to let their fans support them in a privacy-first way. Further, partnerships with 1,000+ brands allow Throne to offer creators and their fans an exceptional commerce experience.

Started in 2021, Throne has revolutionized creator gifting through its Wishlist product. Today, Throne is leveraging the relationships it has built with creators and brands to give creators even more tools to engage with their fans in a safe, fun and enjoyable way. In this way, Throne is a three-sided marketplace connecting world-class brands with creators and their fans. Throne has offices in the US and Germany.

**I have computer with Windows 7, or Window 8, or Windows 8.1. Which version of NekoBox should I download?** <br/>
For Windows 7, Windows 8 and Windows 8.1 it is recommended to install 32 bit NekoBox, even if you run Windows with x64 cpu support. In generally, 64 bit executables does not means run faster than 32 bit unless program needs more than 2Gb of ram memory, which is not the case with NekoBox. 64 bit executables are built with latest Qt and does support Windows 10/11 dark theme. You can read more about 32 bit software at [Reddit topic](https://www.reddit.com/r/AskComputerScience/comments/15aqv4o/are_there_any_advantages_to_32_bit_software_or_64/)

**I got the msvcp140.dll not found error on windows** <br/>
The "msvcp140.dll not found" error usually means that the Microsoft Visual C++ Redistributable is missing or corrupted. To fix this, try install or reinstall the Microsoft Visual C++ Redistributable from the official Microsoft website
[Official Microsoft website for Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170)

**Does NekoBox really need to be run in UAC mode on Windows?** <br/>
No, that is not necessary. NekoBox restarts nekobox_core in UAC mode if TUN mode is selected and NekoBox is not already running with administrator privileges. NekoBox requests UAC confirmation to restart the core.

**Is setting the `SUID` bit really needed on Linux?** <br/>
No, it is not needed, but if `SUID` does not configured properly, the NekoBox will ask for administrator password to order to restart nekobox_core with administrator privilegies, if TUN mode is selected and NekoBox is not already running as root. NekoBox will ask for password for once, and will not configure `SUID`.

**Why does my internet stop working after I force quit NekoBox?** <br/>
If NekoBox is force-quit while `System proxy` is enabled, the process ends immediately and NekoBox cannot reset the proxy. <br/>
Solution:
- Always close NekoBox normally.
- If you force quit by accident, open nekobox again, enable `System proxy`, then disable it- this will reset the settings.

**Why NekoBox uses Noto emoji instead of Twitter emoji? What differences between them two?** <br/>
  Noto Emoji is part of the Noto font family developed by Google. It aims for a clean, simple, and consistent design across various platforms, emphasizing legibility. It is often used in Android systems and Google services. Noto Emoji is designed to support a wide range of characters and symbols, making it suitable for diverse languages and scripts. Also, Noto Emoji is open source, which allows developers to use and modify it freely in their applications.

  Twitter Emoji, also known as Twemoji, has a more colorful and stylized design. It often features more expressive and characterful designs compared to Noto. These emojis are primarily used on Twitter and other platforms where Twitter’s branding is applied. They are designed for use in tweets, direct messages, and other Twitter communications. While Twemoji can be used elsewhere, it is specifically tailored for the Twitter platform, emphasizing a cohesive user experience within Twitter's ecosystem.

  Noto emoji provides better system integration and is part of the larger Noto fonts project, which aims to support all emoji characters defined in Unicode. Generally, Noto emoji are smaller than Twitter emoji. NekoBox does use Noto emoji font because we prefer better system integration over specific stylized design.

**Where are the downloadable route profiles/rulesets coming from?**<br/>
They are located at the [ruleset](https://github.com/qr243vbi/ruleset/tree/routeprofiles) repository.
