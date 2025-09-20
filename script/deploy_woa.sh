#!/bin/bash
set -e

source script/env_deploy.sh
DEST=$DEPLOYMENT/windows-arm64
rm -rf $DEST
mkdir -p $DEST

#### copy exe ####
cp $BUILD/Throne.exe $DEST

cd download-artifact
cd *windows-arm64
tar xvzf artifacts.tgz -C ../../
cd ../..

#### deploy qt & DLL runtime ####
pushd $DEST
windeployqt Throne.exe --no-translations --no-system-d3d-compiler --no-opengl-sw --no-svg --verbose 2
popd

rm -rf $DEST/dxcompiler.dll $DEST/dxil.dll
