package cipher

import (
	"context"
	"net"

	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

type Method interface {
	DialConn(conn net.Conn, destination M.Socksaddr) (net.Conn, error)
	DialEarlyConn(conn net.Conn, destination M.Socksaddr) net.Conn
	DialPacketConn(conn net.Conn) N.NetPacketConn
}

type MethodOptions struct {
	Password string
	Key      []byte
	KeyList  [][]byte
}

type MethodCreator func(ctx context.Context, methodName string, options MethodOptions) (Method, error)
