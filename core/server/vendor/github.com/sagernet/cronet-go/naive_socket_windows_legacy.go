//go:build windows && !go1.25

package cronet

import "net"

func createSocketPair() (cronetFD int, proxyConn net.Conn, err error) {
	return createTCPLoopbackSocketPair()
}
