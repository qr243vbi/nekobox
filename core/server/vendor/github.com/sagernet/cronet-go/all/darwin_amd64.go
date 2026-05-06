//go:build darwin && !ios && amd64

package all

import (
	_ "github.com/sagernet/cronet-go"
	_ "github.com/sagernet/cronet-go/lib/darwin_amd64"
)
