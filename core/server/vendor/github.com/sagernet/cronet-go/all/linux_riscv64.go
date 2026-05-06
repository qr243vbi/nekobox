//go:build linux && !android && riscv64 && !with_musl

package all

import (
	_ "github.com/sagernet/cronet-go"
	_ "github.com/sagernet/cronet-go/lib/linux_riscv64"
)
