//go:build !darwin && !freebsd && !linux && !windows

package tfo

import (
	"context"
	"net"
)

const comptimeListenNoTFO = true

func (*ListenConfig) listenTFO(_ context.Context, _, _ string) (net.Listener, error) {
	return nil, ErrPlatformUnsupported
}
