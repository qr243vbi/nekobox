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
   BUILD="$SRC_ROOT/build"
   GOFLAGS="-mod=vendor $GOFLAGS"
   VERSION_SINGBOX="$(cat $SRC_ROOT/SingBox.Version)"
   LAST_ACTION='rm -rf "$SRC_ROOT"'
   popd
else
  LAST_ACTION="echo fine"
fi 


( . script/build_go.sh; )

cmake -S $SRC_ROOT -B "$BUILD" -GNinja -DNKR_DEFAULT_VERSION="${INPUT_VERSION:-5.0.0}"
cmake --build "$BUILD" -v -j $(nproc)

( . script/deploy_linux64.sh; )

eval "$LAST_ACTION"
