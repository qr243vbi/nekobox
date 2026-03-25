#!/bin/bash -x
specs=(["nekobox"]="PKGBUILD" ["nekobox-git"]="aur_git/PKGBUILD")
for i in 'nekobox' 'nekobox-git'
do
git clone ssh://aur@aur.archlinux.org/nekobox.git "aur-$i"
cp "${specs[$i]}" "aur-$i/PKGBUILD"
pushd "aur-$i"
makepkg --printsrcinfo > .SRCINFO
git add --all
git commit -am "$MESSAGE"
git push --force
popd
done
