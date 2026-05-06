//go:build ios && arm64 && !tvos && iossimulator

package all

import (
	_ "github.com/sagernet/cronet-go"
	_ "github.com/sagernet/cronet-go/lib/ios_arm64_simulator"
)
