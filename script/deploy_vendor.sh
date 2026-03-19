shopt -s globstar
source script/env_deploy.sh
pushd "$SRC_ROOT"

pushd core/server
pushd gen
 ./update_libs.sh
popd

if [[ ! -f srslist.json ]]
then
curl -fLso "$SRC_ROOT/srslist.json" "https://github.com/qr243vbi/ruleset/raw/refs/heads/rule-set/srslist.json"
fi

go mod tidy
go mod vendor
go list -m -f '{{.Version}}' github.com/sagernet/sing-box > "$SRC_ROOT/SingBox.Version"
popd

pushd "$SRC_ROOT"/core/updater
go mod tidy
go mod vendor
popd

if [[ ! -d "$SRC_ROOT"/.git ]]
then
 git init
 git add --all
 git commit -am "initial commit"
fi

echo "[General]" > global.ini
echo "program_version=$INPUT_VERSION" >> global.ini
echo "program_name=NekoBox" >> global.ini

rm -fv **/*.so
rm -fv **/*.a
rm -fv **/*.dll

git add -f srslist* global.ini core/server/{gen/{libcore_service-remote,main_sing,*.go},vendor} SingBox.Version
git -c user.name="a" -c user.email="my@email.org" commit -am "New Update"


if [[ ! -e "$DEPLOYMENT" ]]
then
 mkdir "$DEPLOYMENT"
fi

(git ls-files | grep -v core/server/sing-box  ; (cd core/server/sing-box; git ls-files | sed 's@^@core/server/sing-box/@') ) | tar --transform="s,^,$archive_standalone/,S" -c --xz -f "$DEPLOYMENT/$archive_standalone.tar.xz" -T-
sha256sum "$DEPLOYMENT/$archive_standalone.tar.xz" > "$DEPLOYMENT/$archive_standalone.tar.xz.sha256sum"


git reset --soft HEAD^1
for i in  srslist* global.ini core/server/{gen/{libcore_service-remote,main_sing,*.go},vendor} SingBox.Version
do
git rm -rf "$i"
rm -rf "$i"
done

popd
