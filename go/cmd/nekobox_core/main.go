package main

import (
	"context"
	"fmt"
	_ "unsafe"

	"grpc_server"

	"github.com/Mahdi-zarei/sing-box-extra/boxbox"
	_ "github.com/Mahdi-zarei/sing-box-extra/distro/all"
)

func main() {
	fmt.Println("sing-box:", boxbox.Version)
	fmt.Println()

	testCtx, cancelTests = context.WithCancel(context.Background())
	grpc_server.RunCore(setupCore, &server{})
	return
}
