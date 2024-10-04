package main

import (
	"fmt"
	_ "unsafe"

	"nekobox_core/internal/boxbox"
	"nekobox_core/internal/boxmain"
	_ "nekobox_core/internal/distro/all"
)

func main() {
	fmt.Println("sing-box-extra:", boxbox.Version)
	fmt.Println()

	// sing-box
	boxmain.Main()
}
