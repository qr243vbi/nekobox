//go:build windows && arm64 && with_purego

package all

import (
	_ "github.com/sagernet/cronet-go"
	_ "github.com/sagernet/cronet-go/lib/windows_arm64"
)
