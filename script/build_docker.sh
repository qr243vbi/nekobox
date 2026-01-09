#!/bin/bash
. script/env_deploy.sh
set -e

if [[ -d download-artifact ]]
then
(
 cd download-artifact
 cd *public-public
 tar xvzf artifacts.tgz -C .
 mv deployment/* $DEPLOYMENT
) ||:
fi

if [[ -f $DEPLOYMENT/$archive_standalone.tar.xz ]]
then
   pushd $DEPLOYMENT
   tar -xvf $archive_standalone.tar.xz
   SRC_ROOT=$PWD/$archive_standalone
   popd
fi 

cmake -S $SRC_ROOT -B "$BUILD" -GNinja
cmake --build "$BUILD" -v
( . script/build_go.sh; )
( . script/deploy_linux64.sh; )
