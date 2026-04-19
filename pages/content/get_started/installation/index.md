+++
title = "Installation"
description = "How to install NekoBox and get started."
weight = 1
sort_by = "weight"

[extra]
+++

NekoBox supports Windows, Linux, and macOS. Choose the appropriate installation method for your platform.

## Download the binary

Go to [GitHub Releases](https://github.com/qr243vbi/NekoBox/releases/latest) and download the file for your platform.

### Version matrix

Platform | Architecture | Minimum Version | File Suffix
-- | -- | -- | --
Windows | x64 | Windows 10 | windows64-installer.exe
Windows | x64 | Windows 10 | windows64.zip
Windows | ARM64 | Windows 10 | windows-arm64.zip
Windows | x64 | Windows 7 SP1 | windowslegacy64.zip
Windows | x86 | Windows 7 SP1 | windows32.zip
Linux | x64 | GLIBC 2.34 | linux-amd64.zip
Linux | x64 | GLIBC 2.34 | debian-amd64.deb
Linux | x64 | GLIBC 2.34 | debian-amd64-system-qt.deb
Linux | ARM64 | GLIBC 2.38 | linux-arm64.zip
Linux | ARM64 | GLIBC 2.38 | debian-arm64.deb
Linux | ARM64 | GLIBC 2.34 | debian-arm64-system-qt.deb
macOS | ARM64 | macOS 13 | macos-arm64.zip
macOS | x64 | macOS 13 | macos-amd64.zip
macOS | x64 | macOS 10.15 | macoslegacy-amd64.zip

### Windows

#### Portable (ZIP)

Extract the ZIP file and run `NekoBox.exe`.

#### Installer (.exe)

Run `NekoBox-x.x.x-windows64-installer.exe`.

### Linux

#### Portable (ZIP)

Download the ZIP package:

```bash
unzip NekoBox-x.x.x-linux-*.zip
./NekoBox
```

#### Debian/Ubuntu (.deb)

```bash
sudo dpkg -i NekoBox-x.x.x-debian-*.deb
```

The `-system-qt` version does not bundle Qt libraries and relies on system-installed ones. If the GUI fails to load, try the system-qt version.

### macOS

Extract the ZIP file. Due to Apple's strict security policy, you must remove the quarantine attribute:

```bash
xattr -d com.apple.quarantine /path/to/NekoBox.app
```

To enable built-in privilege escalation, grant `Terminal` Full Disk Access in `System Preferences` → `Security & Privacy` → `Privacy` → `Full Disk Access`.

## Package managers

Distro | Repository
-- | --
Fedora/RHEL | [NekoBox RPM repository](https://parhelia512.github.io/)
Fedora/RHEL | [Terra](https://github.com/terrapkg/packages/tree/frawhide/anda/apps/NekoBox)
openSUSE/SLES | [NekoBox RPM repository](https://parhelia512.github.io/)
Arch Linux | [AUR](https://aur.archlinux.org/packages/NekoBox-bin)
Any distro | [Nix](https://search.nixos.org/packages?channel=unstable&show=NekoBox)
Windows | [Scoop](https://scoop.sh/#/apps?id=b77aee518a6b60c7a582cc24dfb3269c93f697c6&q=NekoBox)
Windows | [WinGet](https://winstall.app/apps/qr243vbi.NekoBox)

## Build from source

```bash
git clone --recursive https://github.com/qr243vbi/NekoBox.git
cd NekoBox
mkdir build
cd build
curl -fLso srslist.h "https://raw.githubusercontent.com/qr243vbi/routeprofiles/rule-set/srslist.h"
cmake ..
make -j$(nproc)
```

## Updating

NekoBox has a built-in update function. You can also download new releases manually from the [Releases page](https://github.com/qr243vbi/NekoBox/releases).

## Troubleshooting

### Antivirus Detection

Some antivirus software may flag NekoBox as malware because its update functionality downloads, removes, and replaces files—similar to ransomware behavior. Additionally, the `System DNS` feature modifies system DNS settings, which some antivirus applications consider dangerous.

## Next Steps

After installation, proceed to [Configuration](/get_started/configuration/) to set up your proxy profiles.
