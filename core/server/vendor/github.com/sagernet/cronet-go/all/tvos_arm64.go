//go:build ios && arm64 && tvos && !tvossimulator

package all

import (
	_ "github.com/sagernet/cronet-go"
	_ "github.com/sagernet/cronet-go/lib/tvos_arm64"
)
