#!/bin/bash
set -e

version="$1"

mkdir -p nekoray/DEBIAN
mkdir -p nekoray/opt
cp -r linux64 nekoray/opt/
mv nekoray/opt/linux64 nekoray/opt/nekoray

# basic
cat >nekoray/DEBIAN/control <<-EOF
Package: nekoray
Version: $version
Architecture: amd64
Maintainer: Mahdi Mahdi.zrei@gmail.com
Depends: libxcb-util1, libqt6core6, libqt6dbus6, libqt6gui6, libqt6network6, libqt6widgets6, libqt6svg6, libicu-dev, libxcb-cursor0, desktop-file-utils
Description: Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
EOF

cat >nekoray/DEBIAN/postinst <<-EOF
if [ ! -s /usr/share/applications/nekoray.desktop ]; then
    cat >/usr/share/applications/nekoray.desktop<<-END
[Desktop Entry]
Name=nekoray
Comment=Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
Exec=sh -c "PATH=/opt/launcher:\$PATH /opt/nekoray/nekobox -appdata"
Icon=/opt/nekoray/nekobox.png
Terminal=false
Type=Application
Categories=Network;Application;
END
fi

setcap cap_net_admin=ep /opt/nekoray/nekobox_core

update-desktop-database
EOF

sudo chmod 0755 nekoray/DEBIAN/postinst

# desktop && PATH

sudo dpkg-deb -Zxz --build nekoray