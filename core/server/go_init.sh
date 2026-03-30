rm go.mod
rm go.sum

go mod init nekobox_core
go mod tidy
echo 'replace github.com/sagernet/sing-box => ./sing-box' >> go.mod
go mod tidy
pushd gen
. update_libs.sh
popd
