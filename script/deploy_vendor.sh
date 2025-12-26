source script/env_deploy.sh

pushd "$SRC_ROOT"/core/server
pushd gen
 ./update_libs.sh
popd
curl -fLso "$SRC_ROOT/srslist.json" "https://github.com/qr243vbi/ruleset/raw/refs/heads/rule-set/srslist.json"
go mod tidy
go mod vendor
go list -m -f '{{.Version}}' github.com/sagernet/sing-box > "$SRC_ROOT/SingBox.Version"
popd

if [[ ! -d "$SRC_ROOT"/.git ]]
then
 git init
 git add --all
 git commit -am "initial commit"
fi

echo $INPUT_VERSION > version.txt

git add -f srslist* version.txt core/server/gen/*.go core/server/gen/libcore_service-remote core/server/vendor SingBox.Version

git -c user.name="qr243vbi" -c user.email="my@email.org" commit -am "New Update"

if [[ ! -e "$DEPLOYMENT" ]]
then
 mkdir "$DEPLOYMENT"
fi
git ls-files --recurse-submodules | tar --transform="s,^,nekobox-unified-source-$INPUT_VERSION/,S" -c --xz -f "$DEPLOYMENT/nekobox-unified-source-$INPUT_VERSION.tar.xz" -T-
sha256sum "$DEPLOYMENT/nekobox-unified-source-$INPUT_VERSION.tar.xz" > "$DEPLOYMENT/nekobox-unified-source-$INPUT_VERSION.tar.xz.sha256sum"

git reset --hard HEAD^1
