source script/env_deploy.sh

pushd "$SRC_ROOT"/core/server
mkdir -p "$DEPLOYMENT"
pushd gen
  thrift --gen go -out "$DEPLOYMENT" libcore.thrift
popd

curl -fLso "$DEPLOYMENT/srslist.json" "https://github.com/qr243vbi/ruleset/raw/refs/heads/rule-set/srslist.json"

go mod tidy
go mod vendor
mv -T vendor "$DEPLOYMENT/vendor"
cp go.mod "$DEPLOYMENT/go.mod"
cp go.sum "$DEPLOYMENT/go.sum"
curl https://api.github.com/repos/sagernet/sing-box/releases/latest | jq -r '.name' > "$DEPLOYMENT/Sagernet.SingBox.Version.txt"
popd

