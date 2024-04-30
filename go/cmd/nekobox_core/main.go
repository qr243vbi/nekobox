package main

import (
	"fmt"
	"os"
	_ "unsafe"

	"grpc_server"

	"github.com/Mahdi-zarei/sing-box-extra/boxbox"
	"github.com/Mahdi-zarei/sing-box-extra/boxmain"
	_ "github.com/Mahdi-zarei/sing-box-extra/distro/all"
	"github.com/matsuridayo/libneko/neko_common"
)

func main() {
	fmt.Println("sing-box-extra:", boxbox.Version, "NekoBox:", neko_common.Version_neko)
	fmt.Println()

	// nekobox_core
	if len(os.Args) > 1 && os.Args[1] == "nekobox" {
		neko_common.RunMode = neko_common.RunMode_NekoBox_Core
		grpc_server.RunCore(setupCore, &server{})
		return
	}

	// sing-box
	boxmain.Main()
}
