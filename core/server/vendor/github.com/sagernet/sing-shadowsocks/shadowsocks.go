package shadowsocks

import (
	"crypto/md5"
	"net"

	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
	F "github.com/sagernet/sing/common/format"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

var (
	ErrBadKey          = E.New("bad key")
	ErrMissingPassword = E.New("missing password")
	ErrNoUsers         = E.New("no users")
)

type Method interface {
	Name() string
	DialConn(conn net.Conn, destination M.Socksaddr) (net.Conn, error)
	DialEarlyConn(conn net.Conn, destination M.Socksaddr) net.Conn
	DialPacketConn(conn net.Conn) N.NetPacketConn
}

type Service interface {
	Name() string
	Password() string
	N.TCPConnectionHandler
	N.UDPHandler
	E.Handler
}

type MultiService[U comparable] interface {
	Name() string
	UpdateUsers(userList []U, keyList [][]byte) error
	UpdateUsersWithPasswords(userList []U, passwordList []string) error
	N.TCPConnectionHandler
	N.UDPHandler
	E.Handler
}

type Handler interface {
	N.TCPConnectionHandler
	N.UDPConnectionHandler
	E.Handler
}

type ServerConnError struct {
	net.Conn
	Source M.Socksaddr
	Cause  error
}

func (e *ServerConnError) Close() error {
	if conn, ok := common.Cast[*net.TCPConn](e.Conn); ok {
		conn.SetLinger(0)
	}
	return e.Conn.Close()
}

func (e *ServerConnError) Unwrap() error {
	return e.Cause
}

func (e *ServerConnError) Error() string {
	return F.ToString("shadowsocks: serve TCP from ", e.Source, ": ", e.Cause)
}

type ServerPacketError struct {
	Source M.Socksaddr
	Cause  error
}

func (e *ServerPacketError) Unwrap() error {
	return e.Cause
}

func (e *ServerPacketError) Error() string {
	return F.ToString("shadowsocks: serve UDP from ", e.Source, ": ", e.Cause)
}

func Key(password []byte, keySize int) []byte {
	var b, prev []byte
	h := md5.New()
	for len(b) < keySize {
		h.Write(prev)
		h.Write(password)
		b = h.Sum(b)
		prev = b[len(b)-h.Size():]
		h.Reset()
	}
	return b[:keySize]
}
