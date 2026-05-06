//go:build linux && !android && 386 && with_musl

package all

import (
	_ "github.com/sagernet/cronet-go"
	_ "github.com/sagernet/cronet-go/lib/linux_386_musl"
)
