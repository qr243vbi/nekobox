#!/bin/bash
cmake -S . -B build -GNinja
cmake --build build --verbose
export GOOS=linux
UNAME="$(uname -m)"

if [[ "${UNAME}" == 'aarch64' || "${UNAME}" == 'arm64' ]]; then
  GOARCH="arm64"
else
  GOARCH="amd64"
fi
export GOARCH

./script/build_go.sh
./script/deploy_linux64.sh
