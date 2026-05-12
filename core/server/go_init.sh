rm go.mod
rm go.sum
rm -rf vendor

shopt -s extglob
shopt -s globstar

#git clone --depth 1 https://github.com/qr243vbi/sing-box
#git clone --depth 1 https://github.com/qr243vbi/sing-tun

go mod init nekobox_core
go mod tidy

go mod edit -replace=github.com/sagernet/sing-box=github.com/qr243vbi/sing-box@HEAD
go mod tidy

go mod edit -replace=github.com/sagernet/sing-tun=github.com/qr243vbi/sing-tun@HEAD
go mod tidy

go mod edit -replace=github.com/sagernet/sing-vmess=github.com/qr243vbi/sing-vmess@HEAD
go mod tidy

go mod edit -replace=github.com/sagernet/gvisor=github.com/nintendobox/gvisor@HEAD  
go mod tidy

go get -u github.com/apache/thrift@HEAD
go mod tidy

qr243vbi_version="$(go list -m -json all | jq -r 'select(.Replace != null) | select (.Replace.Path == "github.com/qr243vbi/sing-box") | .Replace.Version')"
export SING_BOX="$(go env GOPATH)/pkg/mod/github.com/qr243vbi/sing-box@${qr243vbi_version}" 

pushd gen
rm -rf gen/main_sing
. update_libs.sh
popd

go mod edit -go=1.23
go mod tidy

#go mod vendor

rm -rf sing-box
rm -rf sing-tun
rm -rf vendor/**/*.so
rm -rf vendor/**/*.a

cd ../updater
rm go.mod
rm go.sum
rm -rf vendor
go mod init updater
go mod tidy

go mod edit -go=1.23
go mod tidy


#go mod vendor
