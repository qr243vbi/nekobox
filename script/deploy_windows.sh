#!/bin/bash
set -e

source script/env_deploy.sh
export CURDIR="$SRC_ROOT"

if [[ $1 == "new-x86_64" || -z $1 ]]; then
  ARCH="windows64"
  CROSS="windows-amd64"
  INST="$DEPLOYMENT/nekobox_setup"
else if [[ $1 == "new-arm64" || -z $1 ]]; then
  ARCH="windows-arm64"
  CROSS=$ARCH
  INST="$DEPLOYMENT/nekobox_setup_arm64"
fi; fi;

export DEST="$DEPLOYMENT/$ARCH"
mkdir -p "$DEST"

if [[ -d download-artifact ]]
then
(
 cd download-artifact
 cd *"windows-$CROSS"
 tar xvzf artifacts.tgz -C .
 mv "deployment/$ARCH/"* "$DEST"
) ||:
fi

pushd "$SRC_ROOT"

#### get the pdb ####
if [[ "$COMPILER" == "MinGW" ]]
then
curl -fLJO https://github.com/rainers/cv2pdb/releases/download/v0.53/cv2pdb-0.53.zip
7z x cv2pdb-0.53.zip -ocv2pdb
./cv2pdb/cv2pdb64.exe ./build/nekobox.exe ./tmp.exe ./nekobox.pdb
rm -rf cv2pdb-0.53.zip cv2pdb
cd build
strip -s nekobox.exe
cd ..
rm tmp.exe ||:
mv nekobox.pdb $DEST
fi

#### copy srslist ####
if [[ ! -f srslist.json ]]
then
curl -fLso srslist.json "https://github.com/qr243vbi/ruleset/raw/refs/heads/rule-set/srslist.json"
fi
cp srslist.json "$DEST/srslist.json"

#### copy exe ####
cp "$CURDIR/"*.js $DEST
cp "$BUILD/nekobox.exe" "$DEST" || cp "$BUILD/Release/nekobox.exe" "$DEST"
[[ -f "$BUILD/nekobox_core.exe" ]] && cp "$BUILD/nekobox_core.exe" "$DEST" 
[[ -f "$BUILD/updater.exe" ]] && cp "$BUILD/updater.exe" "$DEST"

cp -RT "$CURDIR/res/public" "$DEST/public"
echo "[General]" > "$DEST/global.ini"
echo "software_name=NelBox" >> "$DEST/global.ini"
echo "software_version=$INPUT_VERSION" >> "$DEST/global.ini"

if [[ "$COMPILER" != "MinGW" ]]
then
pushd $DEST
windeployqt nekobox.exe --no-translations --no-system-d3d-compiler --no-compiler-runtime --no-opengl-sw --verbose 2
rm -rf dxcompiler.dll dxil.dll ||:
popd
fi

(
cd "$CURDIR"
pwd
makensis.exe "-DSOFTWARE_VERSION=$INPUT_VERSION" "-DSOFTWARE_NAME=NekoBox" "-DDIRECTORY=$DEST" "-DOUTFILE=$INST" "-NOCD" 'script/windows_installer.nsi'
)

popd
