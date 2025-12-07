cd `dirname "$(realpath $0)"`
rm -rf *.go
rm -rf */*.go
thrift --gen go -out .. libcore.thrift
