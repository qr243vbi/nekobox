#!/bin/bash
. script/env_deploy.sh
set -e
<<<<<<< HEAD

if [[ -d download-artifact ]]
=======
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
>>>>>>> other-repo/main
then
(
 cd download-artifact
 cd *public-public
 tar xvzf artifacts.tgz -C .
 mv deployment/* $DEPLOYMENT
) ||:
fi

<<<<<<< HEAD
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
=======
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
   
>>>>>>> other-repo/main
else
  LAST_ACTION="echo fine"
fi 

<<<<<<< HEAD

( . script/build_go.sh; )

cmake -S $SRC_ROOT -B "$BUILD" -GNinja -DNKR_DEFAULT_VERSION="${INPUT_VERSION:-5.0.0}"
cmake --build "$BUILD" -v -j $(nproc)

( . script/deploy_linux64.sh; )
=======
(
echo "$SRC_ROOT"
cd "$SRC_ROOT"

if [[ "$SKIP_BUILD_GO" != yes ]]
then
(
. script/build_go.sh; 
)
fi

cmake -S $SRC_ROOT -B "$BUILD" -GNinja -DNKR_DEFAULT_VERSION="${INPUT_VERSION:-5.0.0}"
cmake --build "$BUILD" -v -j $(nproc)
(
. script/deploy_linux64.sh; 
)
)
>>>>>>> other-repo/main

eval "$LAST_ACTION"
