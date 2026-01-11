package conn

import (
	"net"
)

type Listener interface {
	ListenPacketCompat(network, address string) (net.PacketConn, error)
}
