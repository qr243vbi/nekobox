text="$(curl -s -H "Accept: application/vnd.github.v3+json" "https://api.github.com/repos/qr243vbi/nekobox/releases/tags/${INPUT_VERSION}")"
asset_x86="$(echo "$text"  | jq '.assets[] | select(.browser_download_url | endswith("windows32-installer.exe"))')"
asset_x64="$(echo "$text"  | jq '.assets[] | select(.browser_download_url | endswith("windows64-installer.exe"))')"
url_x86="$(echo "$asset_x86" | jq -r '.browser_download_url')"
url_x64="$(echo "$asset_x64" | jq -r '.browser_download_url')"
sha_x86="$(echo "$asset_x86" | jq -r '.digest' | sed 's~sha256:~~g')"
sha_x64="$(echo "$asset_x64" | jq -r '.digest' | sed 's~sha256:~~g')"

rm -Rf nekobox
cp -Rf nekobox_chocolatey nekobox

(
cd nekobox

sed -i "s~@URL_x86@~${url_x86}~g;s~@URL_x64@~${url_x64}~g;s~@SHA_x86@~${sha_x86}~g;s~@SHA_x64@~${sha_x64}~g;" tools/chocolateyinstall.ps1
sed -i "s~<version>.*</version>~<version>${INPUT_VERSION}</version>~g;" nekobox.nuspec

choco pack

if [[ -n "$CHOCOLATEY_API_KEY" ]]
then
  choco apikey add --source 'https://push.chocolatey.org/' --key "$CHOCOLATEY_API_KEY"
  choco push nekobox*.nupkg --source 'https://push.chocolatey.org/'
fi
if [[ "${UPLOAD_WITH_GH}" == 'yes' ]]
then
  gh release upload "${INPUT_VERSION}" nekobox*.nupkg
fi
)


