#!/bin/bash
set -e

source script/env_deploy.sh
DEST=$DEPLOYMENT/linux-system-qt-$ARCH
rm -rf $DEST
mkdir -p $DEST

#### copy binary ####
cp $BUILD/Throne $DEST

#### copy Throne.png ####
cp ./res/public/Throne.png $DEST

#### copy Core ####
cd download-artifact
cd *linux-$ARCH
tar xvzf artifacts.tgz -C ../../
cd ../..
cp deployment/linux-$ARCH/Core $DEST
