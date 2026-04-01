#!/bin/bash
. script/env_deploy.sh
set -e
export NEKOBOX_ENV_DEPLOYED=yes

if [[ -z `command -v $GOCMD` ]]
then
 export SKIP_BUILD_GO=yes
fi

if [[ -d "$1" ]]
then
 export BUILD="$1"
 export DEST="$1"
fi

if [[ -d download-artifact && "$SKIP_BUILD_GO" != yes ]]
then
(
 cd download-artifact
 cd *public-public
 tar xvzf artifacts.tgz -C .
 mv deployment/* $DEPLOYMENT
) ||:
fi

echo $archive_standalone

if [[ -f $DEPLOYMENT/$archive_standalone.tar.xz && "$SKIP_BUILD_GO" != yes ]]
then
   pushd $DEPLOYMENT
   tar -xvf $archive_standalone.tar.xz
   ls $PWD
   echo $archive_standalone
   export SRC_ROOT=$PWD/$archive_standalone
#   BUILD="$SRC_ROOT/build"
   export GOFLAGS="-mod=vendor $GOFLAGS"
   export VERSION_SINGBOX="$(cat $SRC_ROOT/SingBox.Version)"
   export LAST_ACTION='rm -rf "$SRC_ROOT"'
   popd
else
  LAST_ACTION="echo fine"
fi 

if [[ "$SKIP_BUILD_GO" != yes ]]
then
  export BUILD_GO_PARTS=ON
else
  export BUILD_GO_PARTS=OFF
fi

(
echo "$SRC_ROOT"
cd "$SRC_ROOT"

cmake -S $SRC_ROOT -B "$BUILD" -GNinja -DNKR_DEFAULT_VERSION="${INPUT_VERSION:-5.0.0}" -DSKIP_UPDATER="${SKIP_UPDATE_BUTTON:-OFF}" -DBUILD_GO_PARTS=ON -DBUILD_GO_PARTS="${BUILD_GO_PARTS}"
cmake --build "$BUILD" -v -j $(nproc)
(
. script/deploy_linux64.sh; 
)
)

eval "$LAST_ACTION"
