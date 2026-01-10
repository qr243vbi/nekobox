#!/bin/bash
if [[ ! -e "$HOME"/.config/osc/oscrc ]]
then
echo '[general]' > oscrc
echo 'apiurl=https://api.opensuse.org' >> oscrc
echo '[https://api.opensuse.org]' >> oscrc
echo "user=${OBS_USER}" >> oscrc
echo "pass=${OBS_PASSWORD}" >> oscrc
echo 'credentials_mgr_class=osc.credentials.ObfuscatedConfigFileCredentialsManager' >> oscrc

install -Dm644 oscrc "$HOME"/.config/osc/oscrc
fi

(
. PKGBUILD
mkdir nekobox_temp
cd nekobox_temp
pkgurl="$(echo ${source[0]} | sed "s@$pkgver@%{version}@g;" )"

osc co home:juzbun:NekoBox/nekobox -o nekobox
osc co network:vpn/nekobox -o nekobox_tumbleweed

(
cd nekobox
echo "$pkgver" > version.txt
osc addremove
osc ci -m update
)

(
cd nekobox_tumbleweed
rm *.tar.xz
rm nekobox.spec

wget https://codefloe.com/qr243vbi/nekobox/raw/branch/main/nekobox.spec
sed -i "s@Source0:.*@Source0: $pkgurl@g; s@Version:.*@Version: $pkgver@g;" nekobox.spec
wget "${source[0]}"
osc addremove
osc ci -m update 
)

)
