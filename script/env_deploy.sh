if [[ -d download-artifact ]]
then
(
cd download-artifact
cd *linux-$ARCH
tar xvzf artifacts.tgz -C ../../
) ||:
fi

if [[ ! -f core/server/sing-box/box.go && -d .git ]]
then
  git submodule init ||:
  git submodule update ||:
fi

SRC_ROOT="$PWD"
DEPLOYMENT="${DEPLOYMENT:-$SRC_ROOT/deployment}"
BUILD="$SRC_ROOT/build"
version_standalone="nekobox-$INPUT_VERSION"
archive_standalone="nekobox-unified-source-$INPUT_VERSION"
