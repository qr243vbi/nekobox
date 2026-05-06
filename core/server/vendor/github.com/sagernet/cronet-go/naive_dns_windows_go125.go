//go:build windows && go1.25

package cronet

import "net"

func createPacketSocketPair(forceUDPLoopback bool) (cronetFD int, proxyConn net.PacketConn, err error) {
	// AF_UNIX SOCK_STREAM on Windows is message-mode: each send() is an
	// atomic message and recv() with a buffer smaller than the message
	// returns WSAEMSGSIZE. This breaks the framed read protocol where
	// C++ reads a 2-byte length header first. Use UDP loopback instead.
	//
	// if forceUDPLoopback {
	// 	return createUDPLoopbackPair()
	// }
	//
	// // Try Unix socket pair first (Go 1.25+ has FileConn support)
	// cronetFD, streamConn, err := createUnixSocketPair()
	// if err != nil {
	// 	return createUDPLoopbackPair()
	// }
	//
	// return cronetFD, newFramedPacketConn(streamConn), nil

	return createUDPLoopbackPair()
}
