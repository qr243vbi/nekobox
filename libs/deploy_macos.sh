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
cd ../..

mv deployment/public_res/* deployment/macos-$ARCH
mv deployment/macos-$ARCH/* $BUILD/nekoray.app/Contents/MacOS

#### deploy qt & DLL runtime => .app ####
pushd $BUILD
macdeployqt nekoray.app -verbose=3
popd

codesign --force --deep --sign - $BUILD/nekoray.app

mv $BUILD/nekoray.app $DEST