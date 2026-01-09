#!/bin/bash
. script/env_deploy.sh
if [[ -d download-artifact ]]
then
(
cd download-artifact
cd *linux-$ARCH
tar xvzf artifacts.tgz -C ../../
) ||:
fi

cmake -S . -B build -GNinja
cmake --build build -v
(
. script/build_go.sh
)
(
. script/deploy_linux64.sh
)
