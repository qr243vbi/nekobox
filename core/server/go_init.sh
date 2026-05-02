rm go.mod
rm go.sum

go mod init nekobox_core
go mod tidy

go mod edit -replace=github.com/sagernet/sing-box=github.com/kindestone/sing-box@latest
go mod tidy

go mod edit -replace=github.com/sagernet/sing-tun=github.com/qr243vbi/sing-tun@latest
go mod tidy

go mod edit -replace=github.com/sagernet/sing-vmess=github.com/qr243vbi/sing-vmess@latest
go mod tidy

qr243vbi_version="$(go list -m -json all | jq -r 'select(.Replace != null) | select (.Replace.Path == "github.com/kindestone/sing-box") | .Replace.Version')"

export SING_BOX="$(go env GOPATH)/pkg/mod/github.com/kindestone/sing-box@${qr243vbi_version}" 

pushd gen
rm -rf gen/main_sing
. update_libs.sh
popd

go mod edit -go=1.23
go mod tidy
