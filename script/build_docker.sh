#!/bin/bash
. script/env_deploy.sh

cmake -S . -B "$BUILD" -GNinja
cmake --build "$BUILD" -v
[[ -n "SKIP_BUILD_GO" ]] || ( . script/build_go.sh; )
( . script/deploy_linux64.sh; )
