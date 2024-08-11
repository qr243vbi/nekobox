#!/bin/bash
set -e

if [[ $(uname -m) == 'aarch64' || $(uname -m) == 'arm64' ]]; then
  ARCH="arm64"
else
  ARCH="amd64"
fi

source libs/env_deploy.sh
DEST=$DEPLOYMENT/linux64
rm -rf $DEST
mkdir -p $DEST

#### copy binary ####
cp $BUILD/nekoray $DEST

cd download-artifact
cd *linux-$ARCH
tar xvzf artifacts.tgz -C ../../
cd ..
cd *public_res
tar xvzf artifacts.tgz -C ../../
cd ../..

mv $DEPLOYMENT/public_res/* $DEST

wget https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20240109-1/linuxdeploy-x86_64.AppImage
wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20240109-1/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage

export EXTRA_QT_PLUGINS="svg;iconengines;"
./linuxdeploy-x86_64.AppImage --appdir $DEST --executable $DEST/nekoray --plugin qt
rm linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage
cd $DEST
rm -r ./usr/translations ./usr/bin ./usr/share ./apprun-hooks

# fix extra libs...
mkdir ./usr/lib2
ls ./usr/lib/
cp ./usr/lib/libQt* ./usr/lib/libxcb-util* ./usr/lib/libicuuc* ./usr/lib/libicui18n* ./usr/lib/libicudata* ./usr/lib/libcpr* ./usr/lib2
rm -r ./usr/lib
mv ./usr/lib2 ./usr/lib
