#!/bin/bash
set -e

source script/env_deploy.sh
if [[ "$GOOS" == "windows" && "$GOARCH" == "amd64" ]]; then
  DEST=$DEPLOYMENT/windows64
else if [[ "$GOOS" == "windows" && "$GOARCH" == "arm64" ]]; then
  DEST=$DEPLOYMENT/windows-arm64
else if [[ "$GOOS" == "windows" && "$GOARCH" == "386" ]]; then
  DEST=$DEPLOYMENT/windowsnew32
else if [[ "$GOOS" == "windowslegacy" && "$GOARCH" == "amd64" ]]; then
  DEST=$DEPLOYMENT/windowslegacy64
else if [[ "$GOOS" == "windowslegacy" && "$GOARCH" == "arm64" ]]; then
  DEST=$DEPLOYMENT/windowslegacy-arm64
else if [[ "$GOOS" == "windowslegacy" && "$GOARCH" == "386" ]]; then
  DEST=$DEPLOYMENT/windows32
else if [[ "$GOOS" == "linux" && "$GOARCH" == "amd64" ]]; then
  DEST=$DEPLOYMENT/linux-amd64
else if [[ "$GOOS" == "linux" && "$GOARCH" == "arm64" ]]; then
  DEST=$DEPLOYMENT/linux-arm64
else if [[ "$GOOS" == "linux" && "$GOARCH" == "386" ]]; then
  DEST=$DEPLOYMENT/linux-i386
fi; fi; fi; fi; fi; fi; fi; fi; fi;

if [ -z $DEST ]; then
  DEST=$PWD/build
fi

echo "DESTINATION IS $DEST FOR MACHINE $GOARCH with platform $GOOS"
TAGS="with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale"

GOCMD="${GOCMD:-go}"

mkdir -p $DEST ||:

export CGO_ENABLED=0

[ "$GOOS" == "windows" ] && EXT=".exe" || EXT=''

#### Go: updater ####
[ "$SKIP_UPDATER" == y ] || (
cd core/updater
[ "$GO_MOD_TIDY" == yes ] && $GOCMD mod tidy
$GOCMD build -o $DEST/updater"${EXT}" -trimpath -ldflags "-w -s"
) ||:

#### Go: core ####
pushd core/server
(
cd gen
protoc -I . --go_out=. --go-grpc_out=. libcore.proto
) || :
VERSION_SINGBOX="${VERSION_SINGBOX:-$(go list -m -f '{{.Version}}' github.com/sagernet/sing-box)}"
[ "$GO_MOD_TIDY" == yes ] && $GOCMD mod tidy
$GOCMD build -v -o $DEST/nekobox_core$EXT -trimpath -ldflags "-w -s -X 'github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX}'" -tags "$TAGS"
popd
