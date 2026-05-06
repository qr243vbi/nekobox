//go:build windows && !go1.25

package cronet

import "net"

func createPacketSocketPair(forceUDPLoopback bool) (cronetFD int, proxyConn net.PacketConn, err error) {
	return createUDPLoopbackPair()
}
