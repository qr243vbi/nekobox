//go:build linux && !android && mips64le && !with_musl

package all

import (
	_ "github.com/sagernet/cronet-go"
	_ "github.com/sagernet/cronet-go/lib/linux_mips64le"
)
