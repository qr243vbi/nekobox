package main

import (
	"fmt"
	_ "unsafe"

	"grpc_server"

	"github.com/Mahdi-zarei/sing-box-extra/boxbox"
	_ "github.com/Mahdi-zarei/sing-box-extra/distro/all"
)

func main() {
	fmt.Println("sing-box:", boxbox.Version)
	fmt.Println()

	grpc_server.RunCore(setupCore, &server{})
	return
}
