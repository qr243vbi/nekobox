//go:build linux && !android && arm64 && !with_musl

package all

import (
	_ "github.com/sagernet/cronet-go"
	_ "github.com/sagernet/cronet-go/lib/linux_arm64"
)
