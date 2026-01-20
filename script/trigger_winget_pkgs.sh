#!/bin/bash

REPO_TO_FORK="microsoft/winget-pkgs"
BASE_BRANCH="master"
USERNAME="qr243vbi"
PROGID="NekoBox"
A='q'
OLDVER='5.10.2'

text="$(curl -s -H "Accept: application/vnd.github.v3+json" https://api.github.com/repos/qr243vbi/nekobox/releases/tags/$INPUT_VERSION)"
asset_res="$(echo "$text"  | jq '.assets[] | select(.browser_download_url | endswith("-installer.exe"))')"

OLD_SHA_32='0AE140E83036DFC0E573A8955D31540149816ABF7FBBAE1E86339AD830F03FC0'
OLD_SHA_64='153A91B21C1D2EC0CA7628FECB01855DC3EEE740F68F42654B295B9CF5A8C79C'
OLD_SHA_ARM64='F58B1EFF96D115FB5125B64E2E4BE532912C19F1236D0B1BC8FEF501FBA59A26'
OLD_DATE='2026-01-03'

PATTERN="s~/S /WINGET=1~/S /NOSCRIPT=1 /WINGET=1~g;s~$OLDVER~$INPUT_VERSION~g;s~$OLD_DATE~$(date +%Y-%m-%d)~g;$(echo $asset_res | jq -r '"\(.name)_\(.digest | ascii_upcase)"' | sed "s~.*windows32.*:\(.*\)~s@$OLD_SHA_32@\1@g;~g;s~.*arm64.*:\(.*\)~s@$OLD_SHA_ARM64@\1@g;~g;s~.*windows64.*:\(.*\)~s@$OLD_SHA_64@\1@g;~g;" | sed ':a;N;$!ba;s/\n//g')"

NEW_BRANCH_NAME="NekoBox-branch-$INPUT_VERSION-$(date +'%Y%m%d%H%M%S')"
git clone --depth 10 "https://${USERNAME}:${GITHUB_TOKEN}@github.com/${USERNAME}/winget-pkgs" ||:

cd "$(basename "$REPO_TO_FORK")" || { echo "Failed to change directory"; exit 1; }

git config user.email "${EMAIL:-github-action@users.noreply.github.com}"
git config user.name "${USERNAME}"

git remote set-url origin "https://${USERNAME}:${GITHUB_TOKEN}@github.com/${USERNAME}/winget-pkgs"

git fetch origin

git checkout "$BASE_BRANCH"
git reset --hard "origin/$BASE_BRANCH"
git reset --hard HEAD^1
git pull

# Create and switch to the new branch
git checkout -b "$NEW_BRANCH_NAME"

abr(){
  mkdir -p "$1" ||:
  cd "$1"
}

abr manifests/${A}/${USERNAME}/${PROGID}/${INPUT_VERSION}
cp ../${OLDVER}/* ./
for i in *
do
 sed -i "$PATTERN" "$i"
done

echo "Successfully set up branch '$NEW_BRANCH_NAME' based on '$BASE_BRANCH'."

git add --all
git commit -am update 

git push --set-upstream origin "$NEW_BRANCH_NAME"

gh repo set-default "$REPO_TO_FORK"
gh pr create --base "$BASE_BRANCH" --head "$USERNAME:$NEW_BRANCH_NAME" --title "New version: qr243vbi.NekoBox version ${INPUT_VERSION}" --body ""

