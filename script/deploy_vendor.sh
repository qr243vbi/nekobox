source script/env_deploy.sh

pushd "$SRC_ROOT"/core/server
mkdir -p "$DEPLOYMENT/gen"
pushd gen
  protoc -I . --go_out="." --protorpc_out="." libcore.proto
  protoc -I . --go_out="$DEPLOYMENT/gen" --protorpc_out="$DEPLOYMENT/gen" libcore.proto
popd

curl -fLso "$DEPLOYMENT/srslist.h" "https://raw.githubusercontent.com/throneproj/routeprofiles/rule-set/srslist.h"
curl -fLso "$DEPLOYMENT/srslist" "https://raw.githubusercontent.com/throneproj/routeprofiles/rule-set/list"         

go mod tidy
go mod vendor
mv -T vendor "$DEPLOYMENT/vendor"
curl https://api.github.com/repos/sagernet/sing-box/releases/latest | jq -r '.name' > "$DEPLOYMENT/Sagernet.SingBox.Version.txt"
popd

