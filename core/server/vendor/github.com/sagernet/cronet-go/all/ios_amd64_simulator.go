//go:build ios && amd64 && !tvos && iossimulator

package all

import (
	_ "github.com/sagernet/cronet-go"
	_ "github.com/sagernet/cronet-go/lib/ios_amd64_simulator"
)
