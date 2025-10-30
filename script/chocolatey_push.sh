text="$(curl -s -H "Accept: application/vnd.github.v3+json" https://api.github.com/repos/qr243vbi/nekobox/releases/tags/"$INPUT_VERSION")"
asset_x86="$(echo "$text"  | jq '.assets[] | select(.browser_download_url | endswith("windows32-installer.exe"))')"
asset_x64="$(echo "$text"  | jq '.assets[] | select(.browser_download_url | endswith("windows64-installer.exe"))')"
url_x86="$(echo "$asset_x86" | jq -r '.browser_download_url')"
url_x64="$(echo "$asset_x64" | jq -r '.browser_download_url')"
sha_x86="$(echo "$asset_x86" | jq -r '.digest' | sed 's~sha256:~~g')"
sha_x64="$(echo "$asset_x64" | jq -r '.digest' | sed 's~sha256:~~g')"

# 'https://github.com/qr243vbi/nekobox/releases/download/@VERSION@/nekobox-@VERSION@-windows32-installer.exe'
# 'https://github.com/qr243vbi/nekobox/releases/download/@VERSION@/nekobox-@VERSION@-windows64-installer.exe'

cp -Rfv nekobox_template nekobox
#choco new nekobox --force --maintainer qr243vbi --version "$INPUT_VERSION" installertype=exe Url64="$url_x64" Url="$url_x86" Checksum="$sha_x86" Checksum64="$sha_x64" SilentArgs='/S /D=C:\tools\nekobox'

if [[ -n "$CHOCOLATEY_API_KEY" ]]
then
choco apikey add --source 'https://push.chocolatey.org/' --key "$CHOCOLATEY_API_KEY"
fi

(
cd nekobox

sed -i "s~@URL_x86@~${url_x86}~g;s~@URL_x64@~${url_x64}~g;s~@SHA_x86@~${sha_x86}~g;s~@SHA_x64@~${sha_x64}~g;" tools/chocolateyinstall.ps1
sed -i "s~<version>.*</version>~<version>${INPUT_VERSION}</version>~g;" nekobox.nuspec

#cat "${nekobox_yaml}" | yq e "(.package.metadata.version) = \"$INPUT_VERSION\" | (.package.metadata.summary) = \"Nekobox For PC\" | (.package.metadata.tags) = \"nekobox proxy\" | (.package.metadata.description) = \"The Original NekoBox Rebranded\" | (.package.metadata.authors) = \"qr243vbi\" | (.package.metadata.projectUrl) = \"https://github.com/qr243vbi/nekobox\""  -p xml -o xml > "${nekobox_yaml}"
#cat "${nekobox_yaml}" | yq e "(.package.metadata.version) = \"$INPUT_VERSION\"" -p xml -o xml > "${nekobox_yaml}"

choco pack
choco push nekobox*.nupkg --source 'https://push.chocolatey.org/'
)


