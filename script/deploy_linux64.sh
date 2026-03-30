#!/bin/bash
set -e
source script/env_deploy.sh
export CURDIR="$SRC_ROOT"

UNAME="${UNAME:-$(uname -m)}"

if [[ "${UNAME}" == 'aarch64' || "${UNAME}" == 'arm64' ]]; then
  ARCH="arm64"
  ARCH1="aarch64"
  NAIVE=true
else if [[ "${UNAME}" == 'amd64' || "${UNAME}" == 'x86_64' ]]; then
  ARCH="amd64"
  ARCH1="x86_64"
  NAIVE=true
else if [[ "${UNAME}" == '386' ]]; then
  ARCH="386"
  ARCH1="i686"
  ARCH2='i386'
  NAIVE=false
else if [[ "${UNAME}" == 'arm' ]]; then
  ARCH="arm"
  ARCH1="armhf"
  NAIVE=false
fi; fi; fi; fi

ARCH2="${ARCH2:-$ARCH1}"

DEST="$DEPLOYMENT/linux-$ARCH"
mkdir -p $DEST ||:

if [[ -d download-artifact ]]
then
(
 cd download-artifact
 cd *"linux-$ARCH"
 tar xvzf artifacts.tgz -C .
 mv deployment/*"linux-$ARCH/"* "$DEST"
) ||:
fi

pushd "$SRC_ROOT"

#### copy srslist ####
if [[ ! -f srslist.json ]]
then
wget -O srslist.json "https://github.com/qr243vbi/ruleset/raw/refs/heads/rule-set/srslist.json"
fi
cp srslist.json "$DEST/srslist.json"

#### copy binary ####
cp "$BUILD/nekobox" "$DEST"

if [[ "$NAIVE" == true ]]
then

if [[ ! -f "libcronet-linux-${ARCH}.so" ]]
then
wget -O "libcronet-linux-${ARCH}.so" "https://github.com/SagerNet/cronet-go/releases/download/$(wget -q -O - https://api.github.com/repos/SagerNet/cronet-go/releases/latest | jq -r .tag_name)/libcronet-linux-${ARCH}.so"
fi
cp "libcronet-linux-${ARCH}.so"  "$DEST/libcronet.so"

fi

[[ -f "$BUILD/nekobox_core" ]] && cp "$BUILD/nekobox_core" "$DEST"
[[ -f "$BUILD/updater" ]] && cp "$BUILD/updater" "$DEST"

#### copy nekobox.png ####
cp ./res/nekobox.ico "$DEST/nekobox.ico"

ls "$DEST"

command -v linuxdeploy  && ln -s `which linuxdeploy` "linuxdeploy-$ARCH2.AppImage" ||:
command -v linuxdeploy-plugin-qt  && ln -s `which linuxdeploy-plugin-qt` "linuxdeploy-plugin-qt-$ARCH2.AppImage" ||:
if command -v appimagetool
then
  ln -s `which appimagetool` appimagetool-$ARCH1.AppImage ||:
#  APPIMAGE_EXTRA_ARGS=()
fi
## else
  [[ -f runtime-${ARCH1} ]]  || wget -O "runtime-${ARCH1}" https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-${ARCH1}
  APPIMAGE_EXTRA_ARGS=(--runtime-file "$CURDIR/runtime-${ARCH1}")
#fi


[[ -f linuxdeploy-$ARCH2.AppImage ]]            || wget -O linuxdeploy-$ARCH2.AppImage              https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH2.AppImage
[[ -f linuxdeploy-plugin-qt-$ARCH2.AppImage ]]  || wget -O linuxdeploy-plugin-qt-$ARCH2.AppImage    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$ARCH2.AppImage
[[ -f appimagetool-${ARCH1}.AppImage ]]         || wget -O appimagetool-$ARCH1.AppImage             https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${ARCH1}.AppImage
chmod +x *.AppImage ||:

export EXTRA_QT_PLUGINS="iconengines;wayland-shell-integration;wayland-decoration-client;"
export QT_PLUGIN_PATH="${QT_PLUGIN_PATH:-$(qmake6 -query QT_INSTALL_PLUGINS)/platforms}"

qmake6 -query QT_INSTALL_PLUGINS
ls $QT_PLUGIN_PATH
echo "extra platform plugins"
for i in `ls $QT_PLUGIN_PATH`
do
if [[ $i == *.so ]]; then
EXTRA_PLATFORM_PLUGINS="${EXTRA_PLATFORM_PLUGINS}$i;"
fi
done
echo $EXTRA_PLATFORM_PLUGINS

export EXTRA_PLATFORM_PLUGINS

(
cd "$CURDIR"
"./linuxdeploy-$ARCH2.AppImage" --appdir $DEST  --executable $DEST/nekobox --plugin qt
)

cd $DEST
#rm -r ./usr/translations ||:
rm -r ./usr/bin ||:
#rm -r ./usr/share ||:
rm -r ./apprun-hooks ||:

# fix plugins rpath
#mkdir ./usr/plugins           ||:
#mkdir ./usr/plugins/platforms ||:
#cp $QT_PLUGIN_PATH/platforms/*.so ./usr/plugins/platforms ||:
#cp -r $QT_PLUGIN_PATH/platformthemes ./usr/plugins ||:
#cp -r $QT_PLUGIN_PATH/imageformats ./usr/plugins ||:
#cp -r $QT_PLUGIN_PATH/iconengines ./usr/plugins ||:
#cp -r $QT_PLUGIN_PATH/wayland-shell-integration ./usr/plugins ||:
#cp -r $QT_PLUGIN_PATH/wayland-decoration-client ./usr/plugins ||:
#cp -r $QT_PLUGIN_PATH/tls ./usr/plugins ||:
patchelf --set-rpath '$ORIGIN/../../lib' ./usr/plugins/platforms/*.so ||:
patchelf --set-rpath '$ORIGIN/../../lib' ./usr/plugins/platformthemes/*.so ||:

# fix extra libs...
#mkdir ./usr/lib2
#ls ./usr/lib/
#cp ./usr/lib/libQt* ./usr/lib/libxcb-cursor* ./usr/lib/libxcb-util* ./usr/lib/libicuuc* ./usr/lib/libicui18n* ./usr/lib/libicudata* ./usr/lib2 ||:
#rm -r ./usr/lib ||:
#mv ./usr/lib2 ./usr/lib

# fix lib rpath
cp -RT "$CURDIR/res/public" "$DEST/public"
cp "$BUILD/"*.qm "$CURDIR/res/languages.txt" "$CURDIR/"*.js "$DEST/public/"

cd "$DEST"
patchelf --set-rpath '$ORIGIN/usr/lib' ./nekobox

shopt -s extglob

rm -rfv nekobox_directory
mkdir nekobox_directory
mv !(nekobox_directory) nekobox_directory
mv nekobox_directory nekobox
tar -czvf $DEPLOYMENT/$version_standalone-linux-${ARCH}.tar.gz nekobox

mkdir -p appimage/AppDir
cd appimage


chmod 755 *

cp -Rfv ../nekobox/!(updater|appimage|nekobox-${ARCH1}.AppImage) ./AppDir
(
cd AppDir
mv nekobox_core .nekobox_core_binary_file
cat << 'EOF' > nekobox_core
#!/bin/sh
exec "$(dirname $0)"/".nekobox_core_binary_file" -argv0 "${APPIMAGE}" "${@}"
EOF
cat << 'EOF' > AppRun
#!/bin/sh
export NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE="${NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE:-nekobox}"
exec "$(dirname $0)"/"${NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE}" "${@}"
EOF
cat << 'EOF' > updater
#!/bin/sh -x
while [[ "$1" != "--" ]]
do
  shift
done
shift

APPIMAGE="${2}"
B=1
OLD=${APPIMAGE}.old.${OLD}.AppImage

while [[ -e "${OLD}" ]];
do
  B=$((B + 1))
  OLD=${APPIMAGE}.old.${OLD}.AppImage
done

mv "${APPIMAGE}" "${OLD}"
cp "${1}" "${APPIMAGE}"
chmod 755 "${APPIMAGE}"
shift
shift
"${APPIMAGE}" "${@}"
EOF
chmod 755 nekobox_core AppRun updater
)
rm ./AppDir/Tun.png ||:
ln -s public/icon.png ./AppDir/Tun.png
cat << EOF > ./AppDir/nekobox.desktop
[Desktop Entry]
Version=1.0
Terminal=false
Type=Application
Name=NekoBox
Categories=Network;
Keywords=Internet;VPN;Proxy;sing-box;
Exec=nekobox
Icon=Tun
EOF
rm -rfv *.old.* ||:
"$CURDIR/appimagetool-${ARCH1}.AppImage" AppDir "$DEPLOYMENT/$version_standalone-${ARCH1}-linux.AppImage" "${APPIMAGE_EXTRA_ARGS[@]}"

cd ../
rm -rfv appimage
rm -rfv nekobox
rmdir $DEST ||:

popd

