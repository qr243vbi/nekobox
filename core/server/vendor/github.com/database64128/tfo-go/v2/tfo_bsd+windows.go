//go:build darwin || freebsd || (windows && tfogo_checklinkname0)

package tfo

import (
	"context"
	"net"
)

func (d *Dialer) dialTFO(ctx context.Context, network, address string, b []byte) (*net.TCPConn, error) {
	if d.Fallback && runtimeDialTFOSupport.load() == dialTFOSupportNone {
		return d.dialAndWriteTCPConn(ctx, network, address, b)
	}
	return d.dialTFOFromSocket(ctx, network, address, b)
}

func dialTCPAddr(network string, laddr, raddr *net.TCPAddr, b []byte) (*net.TCPConn, error) {
	var d Dialer
	d.SetMultipathTCP(false) // Align with [net.DialTCP].
	c, err := d.dialSingle(context.Background(), network, laddr, raddr, b, nil)
	if err != nil {
		return nil, &net.OpError{Op: "dial", Net: network, Source: laddr, Addr: raddr, Err: err}
	}
	c.SetKeepAlive(true)
	c.SetKeepAlivePeriod(defaultTCPKeepAlive)
	return c, nil
}
