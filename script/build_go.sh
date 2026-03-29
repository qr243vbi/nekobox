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

echo "DESTINATION IS $DEST FOR MACHINE $GOARCH with platform $GOOS"

TAGS="with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale,with_shadowtls,with_grpc,with_acme"
if [[ "$GOARCH" == "arm64" || "$GOARCH" == "amd64" ]]
then 
  TAGS="$TAGS,with_naive,with_naive_outbound,with_purego"
fi
LDFLAGS="-w -s -X 'github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX}' -X 'internal/godebug.defaultGODEBUG=multipathtcp=0' -checklinkname=0"

mkdir -p $DEST ||:

export CGO_ENABLED=0
export GOTOOLCHAIN=local

#### Go: updater ####
[ "$SKIP_UPDATER" == y ] || (
cd core/updater
[ "$GO_MOD_TIDY" == yes ] && $GOCMD mod tidy
$GOCMD build -o "$DEST/" -trimpath -ldflags "-w -s"
) ||:

#### Go: core ####
pushd core/server
(
cd gen
./update_libs.sh
) || :
VERSION_SINGBOX="${VERSION_SINGBOX:-$(go list -m -f '{{.Version}}' github.com/sagernet/sing-box)}"
[ "$GO_MOD_TIDY" == yes ] && $GOCMD mod tidy
$GOCMD build -v -o "$DEST/" -trimpath -ldflags "$LDFLAGS" -tags "$TAGS"
popd
