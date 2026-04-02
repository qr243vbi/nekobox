#!/bin/bash
source script/env_deploy.sh
set -e
if [ -z $DEST ]; then
  DEST=$BUILD
if [[ "$GOOS" == "windows" && "$GOARCH" == "amd64" ]]; then
  DEST=$DEPLOYMENT/windows64
else if [[ "$GOOS" == "windows" && "$GOARCH" == "arm64" ]]; then
  DEST=$DEPLOYMENT/windows-arm64
else if [[ "$GOOS" == "windows" && "$GOARCH" == "386" ]]; then
  DEST=$DEPLOYMENT/windows32
else if [[ "$GOOS" == "linux" && "$GOARCH" == "amd64" ]]; then
  DEST=$DEPLOYMENT/linux-amd64
else if [[ "$GOOS" == "linux" && "$GOARCH" == "arm64" ]]; then
  DEST=$DEPLOYMENT/linux-arm64
else if [[ "$GOOS" == "linux" && "$GOARCH" == "386" ]]; then
  DEST=$DEPLOYMENT/linux-386
else if [[ "$GOOS" == "linux" && "$GOARCH" == "arm" ]]; then
  DEST=$DEPLOYMENT/linux-arm
fi; fi; fi; fi; fi; fi; fi;
fi

mkdir -p "${DEST}"

cmake -S ./core -B "${BUILD}" -D"DESTDIR=$(realpath ${DEST})" "-DSKIP_UPDATER=${SKIP_UPDATER}" "-DGO_MOD_VENDOR=${GO_MOD_VENDOR:-OFF}" "-DGO_MOD_TIDY=${GO_MOD_TIDY:-ON}" "-DGOOS=${GOOS}" "-DGOARCH=${GOARCH}"
cmake --build "${BUILD}"
