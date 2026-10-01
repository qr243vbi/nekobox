#!/bin/bash -x
set -e

source script/env_deploy.sh
export CURDIR="$SRC_ROOT"
nekoray=$EXECUTABLE_NAME

if [[ $1 == "x86_64" || -z $1 ]]; then
  ARCH="windows64"
  CROSS="windows-amd64"
  NAIVE="amd64"
  INST="$DEPLOYMENT/nekobox_setup"
else if [[ $1 == "arm64" ]]; then
  ARCH="windows-arm64"
  CROSS=$ARCH
  NAIVE="arm64"
  INST="$DEPLOYMENT/nekobox_setup_arm64"
else if [[ $1 == "i686" || $1 == "x86" ]]; then
  ARCH="windows32"
  CROSS="windows-386"
  NAIVE="false"
  INST="$DEPLOYMENT/nekobox_setup32"
fi; fi; fi;

export DEST="$DEPLOYMENT/$ARCH"
mkdir -p "$DEST"

if [[ -d download-artifact ]]
then
(
 cd download-artifact
 cd *"$CROSS"
 tar xvzf artifacts.tgz -C .
 mv "deployment/$ARCH/"* "$DEST"
)
fi

pushd "$SRC_ROOT"

if [[ ! -f srslist.json ]]
then
curl -fLso srslist.json "https://github.com/qr243vbi/ruleset/raw/refs/heads/rule-set/srslist.json"
fi
cp srslist.json "$DEST/srslist.json"

rel="$BUILD"
if [[ -f "$BUILD/Release/$nekoray.exe" ]]
then
  rel="$BUILD/Release"
fi

cp "$rel/$nekoray.exe" "$DEST"
touch "$rel/nekobox.dll"
cp "$rel/"*.dll "$DEST"

[[ -f "$BUILD/nekobox_core.exe" ]] && cp "$BUILD/nekobox_core.exe" "$DEST"
[[ -f "$BUILD/updater.exe" ]] && cp "$BUILD/updater.exe" "$DEST"

if [[ ! -s "$DEST/nekobox_core.exe" ]]
then
  echo "nekobox_core.exe is missing in $DEST: neither $BUILD nor the golang artifact for $CROSS provided it" >&2
  exit 1
fi

if [[ "$NAIVE" != "false" ]]
then
if [[ ! -f "libcronet-windows-${NAIVE}.dll" ]]
then
curl -L -o "libcronet-windows-${NAIVE}.dll" "https://github.com/SagerNet/cronet-go/releases/download/$(curl -s -L https://api.github.com/repos/SagerNet/cronet-go/releases/latest | jq -r .tag_name)/libcronet-windows-${NAIVE}.dll"
fi
cp "libcronet-windows-${NAIVE}.dll" "$DEST/libcronet.dll"
fi

cp -RT "$CURDIR/res/public" "$DEST/public"
cp "$BUILD/"*.qm "$CURDIR/res/languages.txt" "$DEST/public/"

XRAY_VERSION="26.9.9"
case "$1" in
  x86_64) XRAY_ASSET="Xray-windows-64.zip" ;;
  arm64) XRAY_ASSET="Xray-windows-arm64-v8a.zip" ;;
  i686|x86) XRAY_ASSET="Xray-windows-32.zip" ;;
  *) echo "Unsupported Xray architecture: $1" >&2; exit 1 ;;
esac

XRAY_URL="https://github.com/XTLS/Xray-core/releases/download/v${XRAY_VERSION}/${XRAY_ASSET}"
XRAY_TMP="$DEST/.${XRAY_ASSET}"
curl -fL --retry 5 --retry-delay 2 -o "$XRAY_TMP" "$XRAY_URL"
7z e -y "$XRAY_TMP" "xray.exe" -o"$DEST"
rm -f "$XRAY_TMP"

if [[ ! -s "$DEST/xray.exe" ]]
then
  echo "Bundled Xray-core binary is missing in $DEST" >&2
  exit 1
fi

XRAY_VERSION_OUTPUT="$("$DEST/xray.exe" version 2>&1)"
if ! grep -Eq "Xray[ -]+${XRAY_VERSION}([[:space:]]|$)" <<< "$XRAY_VERSION_OUTPUT"
then
  echo "Bundled Xray version check failed. Expected ${XRAY_VERSION}, got:" >&2
  echo "$XRAY_VERSION_OUTPUT" >&2
  exit 1
fi

echo "Bundled Xray-core verified: ${XRAY_VERSION_OUTPUT}"

if [[ "$COMPILER" != "MinGW" ]]
then
pushd $DEST
windeployqt "$nekoray.exe" --no-translations --no-system-d3d-compiler --no-compiler-runtime --no-opengl-sw --verbose 2
rm -rf dxcompiler.dll dxil.dll ||:
popd
fi

(
cd "$CURDIR"
pwd

rm "$DEST/icu"*.dll ||:

if [[ "$SKIP_UPX" == "false" ]]
then
if command -v upx
then
pushd "$DEST"
upx *.dll *.exe ||:
popd
fi
fi

if [[ "$SKIP_NSIS" != "true" ]]
then
makensis.exe "-DSOFTWARE_VERSION=$INPUT_VERSION" "-DSOFTWARE_NAME=NekoBox" "-DDIRECTORY=$DEST" "-DOUTFILE=$INST" "-NOCD" 'script/windows_installer.nsi'
fi

pushd "$DEPLOYMENT"

if [[ "$SKIP_NSIS" != "true" ]]
then
mv "$INST" "$version_standalone-$ARCH-installer.exe"
fi

if [[ "$SKIP_ZIP" == 'true' ]]
then
mv "$ARCH" "$version_standalone-$ARCH"
else
mv "$ARCH" nekobox
zip -9 -r "$version_standalone-$ARCH.zip" nekobox
rm -rf nekobox
fi

popd
)

popd
