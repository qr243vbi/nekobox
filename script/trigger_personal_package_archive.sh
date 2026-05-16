#!/bin/bash
export pkgver="$INPUT_VERSION"
export source_0="https://github.com/qr243vbi/nekobox/releases/download/$pkgver/nekobox-unified-source-$pkgver.tar.xz"


file_0="$(basename "${source_0}")"

if [[ ! -f "${file_0}" ]]
then
curl -L "${source_0}" -o "${file_0}"
fi

dir="$(mktemp -d)"

tar -xJvf "${file_0}" -C "${dir}"

pushd "${dir}/nekobox-unified-source-${pkgver}"
rm .gitignore
popd

gitdir="$(mktemp -d)"

pushd "${gitdir}"
git clone --depth 1 https://miamosagernaki:${PPA_SSH_KEY}@git.launchpad.net/~miamosagernaki/+git/nekobox
pushd nekobox
shopt -s extglob
shopt -s globstar
shopt -s dotglob
rm -rf !(debian|.git)

python -c "
import os
text = ''

if os.path.exists('debian/changelog'):
 f=open('debian/changelog', 'r')
 text=f.read()
 f.close()

msg='''${MESSAGE}'''
import textwrap

def wrap_preserve_newlines(text, width):
 a = '\n'.join([textwrap.fill(line, width) if line.strip() else '' for line in text.splitlines()])
 a = a.splitlines()
 a = [ '  * ' + i for i in a ]
 return '\n'.join(a)

k='nekobox (${pkgver}-1) unstable; urgency=medium\n\n'

if len(msg) > 0 and k not in text:
 msg = wrap_preserve_newlines(msg, 65)
 k+=msg
 k+='\n\n -- ${USER} <${EMAIL}>  $(date -u '+%a, %d %b %Y %H:%M:%S +0000')\n\n'
 text=k+text
 f=open('debian/changelog', 'w')
 f.write(text)
 f.close()

print(text)
"

cp -R "${dir}/nekobox-unified-source-${pkgver}"/* ./
git add --all
git commit -am "Update to $INPUT_VERSION"

ppa_git push --force
unset PPA_SSH_COMMAND

popd
popd


