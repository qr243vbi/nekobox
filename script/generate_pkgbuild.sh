#!/bin/bash
text="$(curl -s -H "Accept: application/vnd.github.v3+json" https://api.github.com/repos/qr243vbi/nekobox/releases/tags/"$INPUT_VERSION")"
asset_res="$(echo "$text"  | jq '.assets[] | select(.browser_download_url | endswith(".tar.xz"))')"
url_res="$(echo "$asset_res" | jq -r '.browser_download_url' | sed "s~$INPUT_VERSION~\${pkgver}~g")"
sha_res="$(echo "$asset_res" | jq -r '.digest' | sed 's~sha256:~~g')"

sed "s@pkgver=(.*@pkgver=${INPUT_VERSION}@g; s@sha256sums=(.*@sha256sums=(\"$sha_res\")@g; s@source=(.*@source=(\"$url_res\")@g;" ./script/nekobox_aur/PKGBUILD
