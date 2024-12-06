#!/bin/bash
set -e

source script/env_deploy.sh
DEST=$DEPLOYMENT/windows64
rm -rf $DEST
mkdir -p $DEST

DEST7=$DEPLOYMENT/windows7
rm -rf $DEST7
mkdir -p $DEST7

#### copy exe ####
cp $BUILD/nekoray.exe $DEST
cp $BUILD/nekoray.exe $DEST7

cd download-artifact
cd *windows-amd64
tar xvzf artifacts.tgz -C ../../
cd ..
cd *public_res
tar xvzf artifacts.tgz -C ../../
cd ../..

cd download-artifact
cd *windows7-amd64
tar xvzf artifacts.tgz -C ../../
cd ../..

cp $DEPLOYMENT/public_res/* $DEST7
mv $DEPLOYMENT/public_res/* $DEST

#### deploy qt & DLL runtime ####
pushd $DEST
windeployqt nekoray.exe --no-translations --no-system-d3d-compiler --no-compiler-runtime --no-opengl-sw --verbose 2
popd

pushd $DEST7
windeployqt nekoray.exe --no-translations --no-system-d3d-compiler --no-compiler-runtime --no-opengl-sw --verbose 2
popd

rm -rf $DEST/dxcompiler.dll $DEST/dxil.dll $DEST7/dxcompiler.dll $DEST7/dxil.dll