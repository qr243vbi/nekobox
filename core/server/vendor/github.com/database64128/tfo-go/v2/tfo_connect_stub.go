//go:build !darwin && !freebsd && !linux && !(windows && tfogo_checklinkname0)

package tfo

import (
	"context"
	"net"
)

const comptimeDialNoTFO = true

func (d *Dialer) dialTFO(ctx context.Context, network, address string, b []byte) (*net.TCPConn, error) {
	if d.Fallback {
		return d.dialAndWriteTCPConn(ctx, network, address, b)
	}
	return nil, ErrPlatformUnsupported
}

func dialTCPAddr(_ string, _, _ *net.TCPAddr, _ []byte) (*net.TCPConn, error) {
	return nil, ErrPlatformUnsupported
}
