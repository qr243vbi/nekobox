//go:build darwin && !ios && arm64

package all

import (
	_ "github.com/sagernet/cronet-go"
	_ "github.com/sagernet/cronet-go/lib/darwin_arm64"
)
