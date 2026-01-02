#!/bin/bash

# Set variables
REPO_TO_FORK="microsoft/winget-pkgs"
BASE_BRANCH="master"
USERNAME="qr243vbi"
PROGID="NekoBox"
A='q'
OLDVER='5.9.9'

text="$(curl -s -H "Accept: application/vnd.github.v3+json" https://api.github.com/repos/qr243vbi/nekobox/releases/tags/$INPUT_VERSION)"
asset_res="$(echo "$text"  | jq '.assets[] | select(.browser_download_url | endswith("-installer.exe"))')"


PATTERN="s~$OLDVER~$INPUT_VERSION~g;s~2025-12-07~$(date +%Y-%m-%d)~g;$(echo $asset_res | jq -r '"\(.name)_\(.digest | ascii_upcase)"' | sed 's~.*arm64.*:\(.*\)~s@FED32109A84E94FDE258F48EF40366111F4CDAEECA617CC3611217EDD38BC701@\1@g;~g;s~.*windows64.*:\(.*\)~s@AA5AC2B7862929F4F078E81FB71A3286DC457B4B0BE9B84B318441FBBDAF2947@\1@g;~g;' | sed ':a;N;$!ba;s/\n//g')"

          git config --local user.email "github-action@users.noreply.github.com"
          git config --local user.name "GitHub Action"


NEW_BRANCH_NAME="NekoBox-branch-$INPUT_VERSION"    
    git clone --depth 10 "https://${USERNAME}:${GITHUB_TOKEN}@github.com/${USERNAME}/winget-pkgs" ||:

    # Navigate to the local repository directory
    cd "$(basename "$REPO_TO_FORK")" || { echo "Failed to change directory"; exit 1; }

    git remote set-url origin "https://${USERNAME}:${GITHUB_TOKEN}@github.com/${USERNAME}/winget-pkgs"

    # Fetch latest changes from the upstream repository
    git fetch origin

    # Reset the current branch to match the upstream repository's base branch
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

# Notify user of success
echo "Successfully set up branch '$NEW_BRANCH_NAME' based on '$BASE_BRANCH'."

git add --all
git commit -am update 

git push --set-upstream origin "$NEW_BRANCH_NAME"

gh repo set-default "$REPO_TO_FORK"
gh pr create --base "$BASE_BRANCH" --head "$USERNAME:$NEW_BRANCH_NAME" --title "$NEW_BRANCH_NAME" --body ""




