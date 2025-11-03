#!/bin/bash
set -e
export CURDIR=$PWD

UNAME="$(uname -m)"

if [[ "${UNAME}" == 'aarch64' || "${UNAME}" == 'arm64' ]]; then
  ARCH="arm64"
  ARCH1="aarch64"
else
  ARCH="amd64"
  ARCH1="x86_64"
fi

source script/env_deploy.sh
DEST=$DEPLOYMENT/linux-$ARCH
rm -rf $DEST
mkdir -p $DEST

#### copy srslist ####
cp srslist.json $DEST/srslist.json

#### copy binary ####
cp $BUILD/nekobox $DEST

#### copy nekobox.png ####
cp ./res/nekobox.ico $DEST/nekobox.ico

cd download-artifact
cd *linux-$ARCH
tar xvzf artifacts.tgz -C ../../
cd ../..

wget https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20250213-2/linuxdeploy-$ARCH1.AppImage
wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-$ARCH1.AppImage
chmod +x linuxdeploy-$ARCH1.AppImage linuxdeploy-plugin-qt-$ARCH1.AppImage

export EXTRA_QT_PLUGINS="iconengines;wayland-shell-integration;wayland-decoration-client;"
export EXTRA_PLATFORM_PLUGINS="libqwayland.so;"
./linuxdeploy-$ARCH1.AppImage --appdir $DEST --executable $DEST/nekobox --plugin qt
rm linuxdeploy-$ARCH1.AppImage linuxdeploy-plugin-qt-$ARCH1.AppImage
cd $DEST
rm -r ./usr/translations ./usr/bin ./usr/share ./apprun-hooks

# fix plugins rpath
rm -r ./usr/plugins
mkdir ./usr/plugins
mkdir ./usr/plugins/platforms
cp $QT_PLUGIN_PATH/platforms/libqxcb.so ./usr/plugins/platforms
cp $QT_PLUGIN_PATH/platforms/libqwayland.so ./usr/plugins/platforms
cp -r $QT_PLUGIN_PATH/platformthemes ./usr/plugins
cp -r $QT_PLUGIN_PATH/imageformats ./usr/plugins
cp -r $QT_PLUGIN_PATH/iconengines ./usr/plugins
cp -r $QT_PLUGIN_PATH/wayland-shell-integration ./usr/plugins
cp -r $QT_PLUGIN_PATH/wayland-decoration-client ./usr/plugins
cp -r $QT_PLUGIN_PATH/tls ./usr/plugins
patchelf --set-rpath '$ORIGIN/../../lib' ./usr/plugins/platforms/libqxcb.so
patchelf --set-rpath '$ORIGIN/../../lib' ./usr/plugins/platforms/libqwayland.so
patchelf --set-rpath '$ORIGIN/../../lib' ./usr/plugins/platformthemes/libqgtk3.so
patchelf --set-rpath '$ORIGIN/../../lib' ./usr/plugins/platformthemes/libqxdgdesktopportal.so

# fix extra libs...
mkdir ./usr/lib2
ls ./usr/lib/
cp ./usr/lib/libQt* ./usr/lib/libxcb-cursor* ./usr/lib/libxcb-util* ./usr/lib/libicuuc* ./usr/lib/libicui18n* ./usr/lib/libicudata* ./usr/lib2
rm -r ./usr/lib
mv ./usr/lib2 ./usr/lib

# fix lib rpath
cp $CURDIR/*.js $DEST
cp -RT $CURDIR/res/public $DEST/public
echo "$INPUT_VERSION" > $DEST/version.txt

cd $DEST
patchelf --set-rpath '$ORIGIN/usr/lib' ./nekobox
