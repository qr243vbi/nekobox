#!/bin/bash
shopt -s extglob
shopt -s globstar

cd `dirname "$(realpath $0)"`
rm -rf *.go
rm -rf libcore_service-remote

PROTO_COMPILER="${PROTO_COMPILER:-protoc}"

pushd ..
"${PROTO_COMPILER}" --go_out=. --go-grpc_out=. -I$OLDPWD libcore.proto 
popd

if [[ ! -d main_sing ]]
then

mkdir main_sing
cp ${SING_BOX:-../sing-box}/cmd/sing-box/cmd*.go main_sing
#!(*_stub.go) main_sing

for i in $(ls main_sing)
do
sed -i 's/package main/package main_sing/g;' main_sing/$i
done

cat << 'EOF' > main_sing/main_sing.go
package main_sing

import (
        "github.com/sagernet/sing-box/log"
)

func MainFunc() {
        if err := mainCommand.Execute(); err != nil {
                log.Fatal(err)
        }
}
EOF

fi
