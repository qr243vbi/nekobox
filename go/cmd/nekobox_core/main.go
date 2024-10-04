package main

import (
	"context"
	"fmt"
	_ "unsafe"

	"grpc_server"

	"nekobox_core/internal/boxbox"
	_ "nekobox_core/internal/distro/all"
)

func main() {
	fmt.Println("sing-box:", boxbox.Version)
	fmt.Println()

	testCtx, cancelTests = context.WithCancel(context.Background())
	grpc_server.RunCore(setupCore, &server{})
	return
}
