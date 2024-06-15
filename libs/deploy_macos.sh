#!/bin/bash
set -e

if [[ $(uname -m) == 'aarch64' || $(uname -m) == 'arm64' ]]; then
  ARCH="arm64"
else
  ARCH="amd64"
fi

source libs/env_deploy.sh
DEST=$DEPLOYMENT/macos-$ARCH
rm -rf $DEST
mkdir -p $DEST

#### copy golang & public_res => .app ####

cd download-artifact
cd *darwin-$ARCH
tar xvzf artifacts.tgz -C ../../
cd ..
cd *public_res
tar xvzf artifacts.tgz -C ../../
cd ..
cd ..

mv deployment/public_res/* deployment/macos-$ARCH
mv deployment/macos-$ARCH/* $BUILD/nekoray.app/Contents/MacOS

#### deploy qt & DLL runtime => .app ####
pushd $BUILD
macdeployqt nekoray.app -verbose=3
popd

#### pack dmg ###
sudo npm install -g appdmg
appdmg appdmg.json $DEST/nekoray.dmg