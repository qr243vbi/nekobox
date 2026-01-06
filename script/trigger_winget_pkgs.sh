#!/bin/bash

REPO_TO_FORK="microsoft/winget-pkgs"
BASE_BRANCH="master"
USERNAME="qr243vbi"
PROGID="NekoBox"
A='q'
OLDVER='5.9.25'

text="$(curl -s -H "Accept: application/vnd.github.v3+json" https://api.github.com/repos/qr243vbi/nekobox/releases/tags/$INPUT_VERSION)"
asset_res="$(echo "$text"  | jq '.assets[] | select(.browser_download_url | endswith("-installer.exe"))')"


PATTERN="s~/S /WINGET=1~/S /NOSCRIPT=1 /WINGET=1~g;s~$OLDVER~$INPUT_VERSION~g;s~2025-01-03~$(date +%Y-%m-%d)~g;$(echo $asset_res | jq -r '"\(.name)_\(.digest | ascii_upcase)"' | sed 's~.*arm64.*:\(.*\)~s@55DE53FA4B6CA5C3DC4DCB9A0EDE9AB933511C206E1289930DD35075E9C22147@\1@g;~g;s~.*windows64.*:\(.*\)~s@0AC94CBAA96CE3F101B9C8773E7227ABD053BE7DA18A84A3F3D6B6DBD7632CC5@\1@g;~g;' | sed ':a;N;$!ba;s/\n//g')"

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

