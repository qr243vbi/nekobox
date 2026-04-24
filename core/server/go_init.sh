rm go.mod
rm go.sum

go mod init nekobox_core
go mod tidy

go mod edit -replace=github.com/sagernet/sing-box=./sing-box
go mod edit -replace=github.com/sagernet/sing-tun=./sing-tun

go mod edit -replace=github.com/sagernet/sing-vmess=github.com/qr243vbi/sing-vmess@latest

go mod tidy
pushd gen
. update_libs.sh
popd

go mod edit -go=1.23
go mod tidy
