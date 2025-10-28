#!/bin/bash
set -e

source script/env_deploy.sh
if [[ $1 == 'arm64' ]]; then
  ARCH="arm64"
  DEST=$DEPLOYMENT/macos-arm64
else
  ARCH="amd64"
  if [[ $1 == 'x86_64' ]]; then
    DEST=$DEPLOYMENT/macos-amd64
  else
    DEST=$DEPLOYMENT/macoslegacy-amd64
  fi
fi

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
