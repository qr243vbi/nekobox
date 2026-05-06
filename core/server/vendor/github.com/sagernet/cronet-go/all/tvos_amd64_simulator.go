//go:build ios && amd64 && tvos && tvossimulator

package all

import (
	_ "github.com/sagernet/cronet-go"
	_ "github.com/sagernet/cronet-go/lib/tvos_amd64_simulator"
)
