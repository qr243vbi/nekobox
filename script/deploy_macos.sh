#!/bin/bash
set -e

source script/env_deploy.sh
if [[ $1 == 'new-arm64' ]]; then
  ARCH="arm64"
  DEST=$DEPLOYMENT/macos-arm64
else if [[ $1 == 'new-x86_64' ]]; then
  ARCH="amd64"
  DEST=$DEPLOYMENT/macos-amd64
else if [[ $1 == 'x86_64' ]]; then
  ARCH="amd64"
  DEST=$DEPLOYMENT/macoslegacy-amd64
fi; fi; fi

rm -rf $DEST
mkdir -p $DEST

#### copy golang => .app ####
cd download-artifact
cd *darwin-$ARCH
tar xvzf artifacts.tgz -C ../../
cd ../..

mv deployment/macos-$ARCH/* $BUILD/nekobox.app/Contents/MacOS

#### copy srslist ####
cp srslist $BUILD/nekobox.app/Contents/MacOS/srslist

#### deploy qt & DLL runtime => .app ####
pushd $BUILD
macdeployqt nekobox.app -verbose=3
popd

codesign --force --deep --sign - $BUILD/nekobox.app

mv $BUILD/nekobox.app $DEST
