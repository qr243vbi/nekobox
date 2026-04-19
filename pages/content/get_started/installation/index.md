+++
title = "Installation"
description = "How to install NekoBox and get started."
weight = 1
sort_by = "weight"

[extra]
+++

NekoBox supports multiple platforms. Choose the appropriate installation method for your platform.

## Download the binary

Go to [GitHub Releases](https://github.com/qr243vbi/NekoBox/releases/latest) and download the file for your platform.

### Version matrix

Platform | Architecture | Minimum Version | File Suffix
-- | -- | -- | --
Windows | x64   | Windows 10 | windows64-installer.exe
Windows | x64   | Windows 10 | windows64.zip
Windows | x86   | Windows 10 | windows32-installer.exe
Windows | x86   | Windows 10 | windows32.zip
Windows | arm64 | Windows 10 | windows-arm64-installer.exe
Windows | arm64 | Windows 10 | windows-arm64.zip
Linux   | x64   | GLIBC 2.34 | linux-amd64.zip
Linux   | arm64 | GLIBC 2.34 | linux-arm64.zip
Linux   | x86   | GLIBC 2.34 | linux-386.zip
Linux   | arm32 | GLIBC 2.34 | linux-arm.zip
Linux   | x64   | GLIBC 2.34 | linux-x86_64.AppImage
Linux   | arm64 | GLIBC 2.34 | linux-aarch64.AppImage
Linux   | x86   | GLIBC 2.34 | linux-i686.AppImage
Linux   | arm32 | GLIBC 2.34 | linux-armhf.AppImage

### Windows

#### Portable (ZIP)

Extract the ZIP file and run `nekobox.exe`.

#### Installer (.exe)

Run `NekoBox-x.x.x-windows64-installer.exe`.

### Linux

#### Portable (ZIP)

Download the ZIP package:

```bash
unzip NekoBox-x.x.x-linux-*.zip
./NekoBox
```

## Package managers

Distro | Repository
-- | --

## Updating

NekoBox has a built-in update function. You can also download new releases manually from the [Releases page](https://github.com/qr243vbi/NekoBox/releases).

## Troubleshooting

### Antivirus Detection

Some antivirus software may flag NekoBox as malware because its update functionality downloads, removes, and replaces files—similar to ransomware behavior. Additionally, the `System DNS` feature modifies system DNS settings, which some antivirus applications consider dangerous.

## Next Steps

After installation, proceed to [Configuration](/get_started/configuration/) to set up your proxy profiles.
