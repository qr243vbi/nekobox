/*
Copyright (C) 2025  dyhkwong

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
*/

package juicity

import (
	"context"
	"io"
	"net"
	"runtime"
	"sync"

	"github.com/sagernet/quic-go"
	"github.com/sagernet/sing-quic"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	"github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/metadata"
	"github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/tls"
)

type ClientOptions struct {
	Context           context.Context
	Dialer            network.Dialer
	ServerAddress     metadata.Socksaddr
	TLSConfig         tls.Config
	UUID              [16]byte
	Password          string
	CongestionControl string

	allowAllCongestionControl bool // do not export
}

type Client struct {
	ctx               context.Context
	dialer            network.Dialer
	serverAddr        metadata.Socksaddr
	tlsConfig         tls.Config
	quicConfig        *quic.Config
	uuid              [16]byte
	password          string
	congestionControl string

	connAccess sync.Mutex
	conn       *clientQUICConnection
}

func NewClient(options ClientOptions) (*Client, error) {
	quicConfig := &quic.Config{
		DisablePathMTUDiscovery: !(runtime.GOOS == "windows" || runtime.GOOS == "linux" || runtime.GOOS == "android" || runtime.GOOS == "darwin"),
		MaxIncomingUniStreams:   1 << 60,
	}
	switch options.CongestionControl {
	case "":
		options.CongestionControl = "bbr"
	case "cubic", "new_reno", "bbr", "bbr2":
	default:
		if !options.allowAllCongestionControl {
			return nil, exceptions.New("unknown congestion control algorithm: ", options.CongestionControl)
		}
	}
	return &Client{
		ctx:               options.Context,
		dialer:            options.Dialer,
		serverAddr:        options.ServerAddress,
		tlsConfig:         options.TLSConfig, // clients need to set ALPN `h3` themselves
		quicConfig:        quicConfig,
		uuid:              options.UUID,
		password:          options.Password,
		congestionControl: options.CongestionControl,
	}, nil
}

func (c *Client) offer(ctx context.Context) (*clientQUICConnection, error) {
	c.connAccess.Lock()
	defer c.connAccess.Unlock()
	conn := c.conn
	if conn != nil && conn.active() {
		return conn, nil
	}
	conn, err := c.offerNew(ctx)
	if err != nil {
		return nil, err
	}
	return conn, nil
}

func (c *Client) offerNew(_ context.Context) (*clientQUICConnection, error) {
	udpConn, err := c.dialer.DialContext(c.ctx, "udp", c.serverAddr)
	if err != nil {
		return nil, err
	}
	var quicConn *quic.Conn
	quicConn, err = qtls.Dial(c.ctx, bufio.NewUnbindPacketConn(udpConn), udpConn.RemoteAddr(), c.tlsConfig, c.quicConfig)
	if err != nil {
		udpConn.Close()
		return nil, exceptions.Cause(err, "open connection")
	}
	setCongestion(c.ctx, quicConn, c.congestionControl)
	conn := &clientQUICConnection{
		quicConn: quicConn,
		rawConn:  udpConn,
		connDone: make(chan struct{}),
	}
	go func() {
		hErr := c.clientHandshake(quicConn)
		if hErr != nil {
			conn.closeWithError(hErr)
		}
	}()
	c.conn = conn
	return conn, nil
}

func (c *Client) clientHandshake(conn *quic.Conn) error {
	authStream, err := conn.OpenUniStream()
	if err != nil {
		return exceptions.Cause(err, "open handshake stream")
	}
	defer authStream.Close()
	handshakeState := conn.ConnectionState()
	authToken, err := handshakeState.TLS.ExportKeyingMaterial(string(c.uuid[:]), []byte(c.password), 32)
	if err != nil {
		return exceptions.Cause(err, "export keying material")
	}
	authRequest := buf.NewSize(AuthenticateLen)
	authRequest.WriteByte(Version)
	authRequest.WriteByte(CommandAuthenticate)
	authRequest.Write(c.uuid[:])
	authRequest.Write(authToken)
	return common.Error(authStream.Write(authRequest.Bytes()))
}

func (c *Client) DialConn(ctx context.Context, destination metadata.Socksaddr) (net.Conn, error) {
	conn, err := c.offer(ctx)
	if err != nil {
		return nil, err
	}
	stream, err := conn.quicConn.OpenStream()
	if err != nil {
		return nil, err
	}
	return &clientConn{
		Stream:      stream,
		parent:      conn,
		destination: destination,
		network:     NetworkTCP,
	}, nil
}

func (c *Client) ListenPacket(ctx context.Context, destination metadata.Socksaddr) (net.PacketConn, error) {
	conn, err := c.offer(ctx)
	if err != nil {
		return nil, err
	}
	stream, err := conn.quicConn.OpenStream()
	if err != nil {
		return nil, err
	}
	return &udpPacketConn{
		Conn: &clientConn{
			Stream:      stream,
			parent:      conn,
			destination: destination,
			network:     NetworkUDP,
		},
	}, nil
}

func (c *Client) CloseWithError(err error) error {
	c.connAccess.Lock()
	defer c.connAccess.Unlock()
	conn := c.conn
	if conn != nil {
		conn.closeWithError(err)
	}
	return nil
}

type clientQUICConnection struct {
	quicConn  *quic.Conn
	rawConn   io.Closer
	closeOnce sync.Once
	connDone  chan struct{}
	connErr   error
}

func (c *clientQUICConnection) active() bool {
	select {
	case <-c.quicConn.Context().Done():
		return false
	default:
	}
	select {
	case <-c.connDone:
		return false
	default:
	}
	return true
}

func (c *clientQUICConnection) closeWithError(err error) {
	c.closeOnce.Do(func() {
		c.connErr = err
		close(c.connDone)
		_ = c.quicConn.CloseWithError(0, "")
		_ = c.rawConn.Close()
	})
}

type clientConn struct {
	*quic.Stream
	parent         *clientQUICConnection
	destination    metadata.Socksaddr
	requestWritten bool
	network        int
}

func (c *clientConn) Read(b []byte) (n int, err error) {
	n, err = c.Stream.Read(b)
	return n, qtls.WrapError(err)
}

func (c *clientConn) Write(b []byte) (n int, err error) {
	if !c.requestWritten {
		request := buf.NewSize(1 + AddressSerializer.AddrPortLen(c.destination) + len(b))
		defer request.Release()
		request.WriteByte(byte(c.network))
		err = AddressSerializer.WriteAddrPort(request, c.destination)
		if err != nil {
			return
		}
		request.Write(b)
		_, err = c.Stream.Write(request.Bytes())
		if err != nil {
			c.parent.closeWithError(exceptions.Cause(err, "create new connection"))
			return 0, qtls.WrapError(err)
		}
		c.requestWritten = true
		return len(b), nil
	}
	n, err = c.Stream.Write(b)
	return n, qtls.WrapError(err)
}

func (c *clientConn) Close() error {
	c.Stream.CancelRead(0)
	return c.Stream.Close()
}

func (c *clientConn) LocalAddr() net.Addr {
	return metadata.Socksaddr{}
}

func (c *clientConn) RemoteAddr() net.Addr {
	return c.destination
}
