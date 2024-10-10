#!/bin/bash
set -e

source libs/env_deploy.sh
[ "$GOOS" == "windows" ] && [ "$GOARCH" == "amd64" ] && DEST=$DEPLOYMENT/windows64 || true
[ "$GOOS" == "windows" ] && [ "$GOARCH" == "arm64" ] && DEST=$DEPLOYMENT/windows-arm64 || true
[ "$GOOS" == "windows7" ] && [ "$GOARCH" == "amd64" ] && DEST=$DEPLOYMENT/windows7 || true
[ "$GOOS" == "linux" ] && [ "$GOARCH" == "amd64" ] && DEST=$DEPLOYMENT/linux64 || true
[ "$GOOS" == "linux" ] && [ "$GOARCH" == "arm64" ] && DEST=$DEPLOYMENT/linux-arm64 || true
[ "$GOOS" == "darwin" ] && [ "$GOARCH" == "amd64" ] && DEST=$DEPLOYMENT/macos-amd64 || true
[ "$GOOS" == "darwin" ] && [ "$GOARCH" == "arm64" ] && DEST=$DEPLOYMENT/macos-arm64 || true
if [ $GOOS = "windows7" ]; then
  GOOS=windows
  OLD=y
fi

if [ -z $DEST ]; then
  echo "Please set GOOS GOARCH"
  exit 1
fi
rm -rf $DEST
mkdir -p $DEST

export CGO_ENABLED=0

#### Go: updater ####
pushd go/cmd/updater
[ "$GOOS" == "darwin" ] || go build -o $DEST -trimpath -ldflags "-w -s"
popd

#### Go: nekobox_core ####
pushd go/cmd/nekobox_core
if [ -z $OLD ]; then
  go build -v -o $DEST -trimpath -ldflags "-w -s -X $neko_common.Version_neko=$version_standalone" -tags "with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_ech,with_dhcp"
else
  go build -v -o $DEST -trimpath -ldflags "-w -s -X $neko_common.Version_neko=$version_standalone" -tags "with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_dhcp"
fi
popd
