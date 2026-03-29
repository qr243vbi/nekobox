#!/bin/bash -x

declare -A specs=(["nekobox"]="PKGBUILD" ["nekobox-git"]="aur_git/PKGBUILD")

for i in 'nekobox' 'nekobox-git'
do
echo "${specs[$i]}"
git clone ssh://aur@aur.archlinux.org/nekobox.git "aur-$i"
cp "${specs[$i]}" "aur-$i/PKGBUILD"
pushd "aur-$i"

if command -v "makepkg"
then
makepkg --printsrcinfo > .SRCINFO
git add --all
git commit -am "$MESSAGE"
git push --force
fi

popd
done
