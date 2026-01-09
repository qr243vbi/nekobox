if [[ ! -f core/server/sing-box/box.go && -d .git ]]
then
  git submodule init ||:
  git submodule update ||:
fi
. .ENV ||: 
SRC_ROOT="${SRC_ROOT:-$PWD}"
DEPLOYMENT="${DEPLOYMENT:-$SRC_ROOT/deployment}"
BUILD="${BUILD:-$SRC_ROOT/build}"
version_standalone="nekobox-$INPUT_VERSION"
archive_standalone="nekobox-unified-source-$INPUT_VERSION"
