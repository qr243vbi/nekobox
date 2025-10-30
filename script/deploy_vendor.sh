source script/env_deploy.sh

pushd "$SRC_ROOT"/core/server
mkdir -p "$DEPLOYMENT/gen"
pushd gen
  protoc -I . --go_out="." --protorpc_out="." libcore.proto
  protoc -I . --go_out="$DEPLOYMENT/gen" --protorpc_out="$DEPLOYMENT/gen" libcore.proto
popd

curl -fLso "$DEPLOYMENT/srslist" "https://github.com/qr243vbi/ruleset/raw/refs/heads/rule-set/srslist"

go mod tidy
go mod vendor
mv -T vendor "$DEPLOYMENT/vendor"
mv go.mod "$DEPLOYMENT/go.mod"
mv go.sum "$DEPLOYMENT/go.sum"
curl https://api.github.com/repos/sagernet/sing-box/releases/latest | jq -r '.name' > "$DEPLOYMENT/Sagernet.SingBox.Version.txt"
popd

