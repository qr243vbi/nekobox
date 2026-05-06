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
	"bytes"
	"context"
	"errors"
	"io"
	"net"
	"runtime"
	"sync"
	"time"

	"github.com/gofrs/uuid/v5"
	"github.com/sagernet/quic-go"
	"github.com/sagernet/sing-quic"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/auth"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	"github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
	"github.com/sagernet/sing/common/metadata"
	"github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/tls"
)

type ServiceOptions struct {
	Context           context.Context
	Logger            logger.Logger
	TLSConfig         tls.ServerConfig
	CongestionControl string
	AuthTimeout       time.Duration
	// UDPTimeout  time.Duration todo?
	Handler ServiceHandler

	allowAllCongestionControl bool // do not export
}

type ServiceHandler interface {
	network.TCPConnectionHandlerEx
	network.UDPConnectionHandlerEx
}

type Service[U comparable] struct {
	ctx               context.Context
	logger            logger.Logger
	tlsConfig         tls.ServerConfig
	quicConfig        *quic.Config
	userMap           map[[16]byte]U
	passwordMap       map[U]string
	congestionControl string
	authTimeout       time.Duration
	handler           ServiceHandler

	quicListener io.Closer
}

func NewService[U comparable](options ServiceOptions) (*Service[U], error) {
	if options.AuthTimeout == 0 {
		// Official Juicity server uses 10 seconds
		options.AuthTimeout = 10 * time.Second
	}
	quicConfig := &quic.Config{
		DisablePathMTUDiscovery: !(runtime.GOOS == "windows" || runtime.GOOS == "linux" || runtime.GOOS == "android" || runtime.GOOS == "darwin"),
		EnableDatagrams:         true,
		MaxIncomingStreams:      1 << 60,
		MaxIncomingUniStreams:   1 << 60,
		DisablePathManager:      true,
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
	return &Service[U]{
		ctx:               options.Context,
		logger:            options.Logger,
		tlsConfig:         options.TLSConfig, // servers need to set ALPN `h3` themselves
		quicConfig:        quicConfig,
		userMap:           make(map[[16]byte]U),
		congestionControl: options.CongestionControl,
		authTimeout:       options.AuthTimeout,
		handler:           options.Handler,
	}, nil
}

func (s *Service[U]) UpdateUsers(userList []U, uuidList [][16]byte, passwordList []string) {
	userMap := make(map[[16]byte]U)
	passwordMap := make(map[U]string)
	for index := range userList {
		userMap[uuidList[index]] = userList[index]
		passwordMap[userList[index]] = passwordList[index]
	}
	s.userMap = userMap
	s.passwordMap = passwordMap
}

func (s *Service[U]) Start(conn net.PacketConn) error {
	listener, err := qtls.Listen(conn, s.tlsConfig, s.quicConfig)
	if err != nil {
		return err
	}
	s.quicListener = listener
	go func() {
		for {
			connection, hErr := listener.Accept(s.ctx)
			if hErr != nil {
				if exceptions.IsClosedOrCanceled(hErr) || errors.Is(hErr, quic.ErrServerClosed) {
					s.logger.Debug(exceptions.Cause(hErr, "listener closed"))
				} else {
					s.logger.Error(exceptions.Cause(hErr, "listener closed"))
				}
				return
			}
			go s.handleConnection(connection)
		}
	}()
	return nil
}

func (s *Service[U]) Close() error {
	return common.Close(
		s.quicListener,
	)
}

func (s *Service[U]) handleConnection(connection *quic.Conn) {
	setCongestion(s.ctx, connection, s.congestionControl)
	session := &serverSession[U]{
		Service:  s,
		ctx:      s.ctx,
		quicConn: connection,
		connDone: make(chan struct{}),
		authDone: make(chan struct{}),
	}
	session.handle()
}

type serverSession[U comparable] struct {
	*Service[U]
	ctx        context.Context
	quicConn   *quic.Conn
	connAccess sync.Mutex
	connDone   chan struct{}
	connErr    error
	authDone   chan struct{}
	authUser   U
}

func (s *serverSession[U]) handle() {
	if s.ctx.Done() != nil {
		go func() {
			select {
			case <-s.ctx.Done():
				s.closeWithError(s.ctx.Err())
			case <-s.connDone:
			}
		}()
	}
	go s.loopUniStreams()
	go s.loopStreams()
	go s.handleAuthTimeout()
}

func (s *serverSession[U]) loopUniStreams() {
	for {
		uniStream, err := s.quicConn.AcceptUniStream(s.ctx)
		if err != nil {
			return
		}
		go func() {
			err = s.handleUniStream(uniStream)
			if err != nil {
				s.closeWithError(exceptions.Cause(err, "handle uni stream"))
			}
		}()
	}
}

func (s *serverSession[U]) handleUniStream(stream *quic.ReceiveStream) error {
	defer stream.CancelRead(0)
	buffer := buf.New()
	defer buffer.Release()
	_, err := buffer.ReadAtLeastFrom(stream, 2)
	if err != nil {
		return exceptions.Cause(err, "read request")
	}
	version := buffer.Byte(0)
	if version != Version {
		return exceptions.New("unknown version ", buffer.Byte(0))
	}
	command := buffer.Byte(1)
	switch command {
	case CommandAuthenticate:
		select {
		case <-s.authDone:
			return exceptions.New("authentication: multiple authentication requests")
		default:
		}
		if buffer.Len() < AuthenticateLen {
			_, err = buffer.ReadFullFrom(stream, AuthenticateLen-buffer.Len())
			if err != nil {
				return exceptions.Cause(err, "authentication: read request")
			}
		}
		var userUUID [16]byte
		copy(userUUID[:], buffer.Range(2, 2+16))
		user, loaded := s.userMap[userUUID]
		if !loaded {
			return exceptions.New("authentication: unknown user ", uuid.UUID(userUUID))
		}
		handshakeState := s.quicConn.ConnectionState()
		token, err := handshakeState.TLS.ExportKeyingMaterial(string(userUUID[:]), []byte(s.passwordMap[user]), 32)
		if err != nil {
			return exceptions.Cause(err, "authentication: export keying material")
		}
		if !bytes.Equal(token, buffer.Range(2+16, AuthenticateLen)) {
			return exceptions.New("authentication: token mismatch")
		}
		s.authUser = user
		close(s.authDone)
		return nil
	default:
		return exceptions.New("unknown command ", command)
	}
}

func (s *serverSession[U]) handleAuthTimeout() {
	select {
	case <-s.connDone:
	case <-s.authDone:
	case <-time.After(s.authTimeout):
		s.closeWithError(exceptions.New("authentication timeout"))
	}
}

func (s *serverSession[U]) loopStreams() {
	for {
		stream, err := s.quicConn.AcceptStream(s.ctx)
		if err != nil {
			return
		}
		go func() {
			err = s.handleStream(stream)
			if err != nil {
				stream.CancelRead(0)
				stream.Close()
				s.logger.Error(exceptions.Cause(err, "handle stream request"))
			}
		}()
	}
}

func (s *serverSession[U]) handleStream(stream *quic.Stream) error {
	// Most of the vulnerabilities described in https://github.com/tuic-protocol/tuic/issues/67#issuecomment-1196862427 are still valid.
	// Unable to fix because they are design flaw.
	buffer := buf.NewSize(1 + metadata.MaxSocksaddrLength)
	defer buffer.Release()
	_, err := buffer.ReadAtLeastFrom(stream, 1)
	if err != nil {
		return exceptions.Cause(err, "read request")
	}
	network, _ := buffer.ReadByte()
	if network != NetworkTCP && network != NetworkUDP {
		return exceptions.New("unsupported stream network")
	}
	destination, err := AddressSerializer.ReadAddrPort(io.MultiReader(buffer, stream))
	if err != nil {
		return exceptions.Cause(err, "read request destination")
	}
	select {
	case <-s.connDone:
		return s.connErr
	case <-s.authDone:
	}
	var conn net.Conn = &serverConn{
		Stream:      stream,
		destination: destination,
	}
	if !buffer.IsEmpty() {
		conn = bufio.NewCachedConn(conn, buffer.ToOwned())
	}
	switch network {
	case NetworkTCP:
		s.handler.NewConnectionEx(auth.ContextWithUser(s.ctx, s.authUser), conn, metadata.SocksaddrFromNet(s.quicConn.RemoteAddr()).Unwrap(), destination, nil)
	case NetworkUDP:
		s.handler.NewPacketConnectionEx(auth.ContextWithUser(s.ctx, s.authUser), &udpPacketConn{Conn: conn}, metadata.SocksaddrFromNet(s.quicConn.RemoteAddr()).Unwrap(), destination, nil)
	}
	return nil
}

func (s *serverSession[U]) closeWithError(err error) {
	s.connAccess.Lock()
	defer s.connAccess.Unlock()
	select {
	case <-s.connDone:
		return
	default:
		s.connErr = err
		close(s.connDone)
	}
	if exceptions.IsClosedOrCanceled(err) {
		s.logger.Debug(exceptions.Cause(err, "connection failed"))
	} else {
		s.logger.Error(exceptions.Cause(err, "connection failed"))
	}
	_ = s.quicConn.CloseWithError(0, "")
}

type serverConn struct {
	*quic.Stream
	destination metadata.Socksaddr
}

func (c *serverConn) Read(p []byte) (n int, err error) {
	n, err = c.Stream.Read(p)
	return n, qtls.WrapError(err)
}

func (c *serverConn) Write(p []byte) (n int, err error) {
	n, err = c.Stream.Write(p)
	return n, qtls.WrapError(err)
}

func (c *serverConn) LocalAddr() net.Addr {
	return c.destination
}

func (c *serverConn) RemoteAddr() net.Addr {
	return metadata.Socksaddr{}
}

func (c *serverConn) Close() error {
	c.Stream.CancelRead(0)
	return c.Stream.Close()
}
