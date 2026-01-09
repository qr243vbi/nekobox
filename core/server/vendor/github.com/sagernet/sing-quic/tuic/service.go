package tuic

import (
	"bytes"
	"context"
	"encoding/binary"
	"errors"
	"io"
	"net"
	"runtime"
	"sync"
	"time"

	"github.com/sagernet/quic-go"
	"github.com/sagernet/sing-quic"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/auth"
	"github.com/sagernet/sing/common/baderror"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	aTLS "github.com/sagernet/sing/common/tls"

	"github.com/gofrs/uuid/v5"
)

type ServiceOptions struct {
	Context           context.Context
	Logger            logger.Logger
	TLSConfig         aTLS.ServerConfig
	CongestionControl string
	AuthTimeout       time.Duration
	ZeroRTTHandshake  bool
	Heartbeat         time.Duration
	UDPTimeout        time.Duration
	Handler           ServiceHandler
}

type ServiceHandler interface {
	N.TCPConnectionHandlerEx
	N.UDPConnectionHandlerEx
}

type Service[U comparable] struct {
	ctx               context.Context
	logger            logger.Logger
	tlsConfig         aTLS.ServerConfig
	heartbeat         time.Duration
	quicConfig        *quic.Config
	userMap           map[[16]byte]U
	passwordMap       map[U]string
	congestionControl string
	authTimeout       time.Duration
	udpTimeout        time.Duration
	handler           ServiceHandler

	quicListener io.Closer
}

func NewService[U comparable](options ServiceOptions) (*Service[U], error) {
	if options.AuthTimeout == 0 {
		options.AuthTimeout = 3 * time.Second
	}
	if options.Heartbeat == 0 {
		options.Heartbeat = 10 * time.Second
	}
	quicConfig := &quic.Config{
		DisablePathMTUDiscovery: !(runtime.GOOS == "windows" || runtime.GOOS == "linux" || runtime.GOOS == "android" || runtime.GOOS == "darwin"),
		EnableDatagrams:         true,
		Allow0RTT:               options.ZeroRTTHandshake,
		MaxIncomingStreams:      1 << 60,
		MaxIncomingUniStreams:   1 << 60,
		DisablePathManager:      true,
	}
	switch options.CongestionControl {
	case "":
		options.CongestionControl = "cubic"
	case "cubic", "new_reno", "bbr":
	default:
		return nil, E.New("unknown congestion control algorithm: ", options.CongestionControl)
	}
	return &Service[U]{
		ctx:               options.Context,
		logger:            options.Logger,
		tlsConfig:         options.TLSConfig,
		heartbeat:         options.Heartbeat,
		quicConfig:        quicConfig,
		userMap:           make(map[[16]byte]U),
		congestionControl: options.CongestionControl,
		authTimeout:       options.AuthTimeout,
		udpTimeout:        options.UDPTimeout,
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
	if !s.quicConfig.Allow0RTT {
		listener, err := qtls.Listen(conn, s.tlsConfig, s.quicConfig)
		if err != nil {
			return err
		}
		s.quicListener = listener
		go func() {
			for {
				connection, hErr := listener.Accept(s.ctx)
				if hErr != nil {
					if E.IsClosedOrCanceled(hErr) || errors.Is(hErr, quic.ErrServerClosed) {
						s.logger.Debug(E.Cause(hErr, "listener closed"))
					} else {
						s.logger.Error(E.Cause(hErr, "listener closed"))
					}
					return
				}
				go s.handleConnection(connection)
			}
		}()
	} else {
		listener, err := qtls.ListenEarly(conn, s.tlsConfig, s.quicConfig)
		if err != nil {
			return err
		}
		s.quicListener = listener
		go func() {
			for {
				connection, hErr := listener.Accept(s.ctx)
				if hErr != nil {
					if E.IsClosedOrCanceled(hErr) || errors.Is(hErr, quic.ErrServerClosed) {
						s.logger.Debug(E.Cause(hErr, "listener closed"))
					} else {
						s.logger.Error(E.Cause(hErr, "listener closed"))
					}
					return
				}
				go s.handleConnection(connection)
			}
		}()
	}
	return nil
}

func (s *Service[U]) Close() error {
	return common.Close(
		s.quicListener,
	)
}

func (s *Service[U]) handleConnection(connection quic.Connection) {
	setCongestion(s.ctx, connection, s.congestionControl)
	session := &serverSession[U]{
		Service:    s,
		ctx:        s.ctx,
		quicConn:   connection,
		connDone:   make(chan struct{}),
		authDone:   make(chan struct{}),
		udpConnMap: make(map[uint16]*udpPacketConn),
	}
	session.handle()
}

type serverSession[U comparable] struct {
	*Service[U]
	ctx        context.Context
	quicConn   quic.Connection
	connAccess sync.Mutex
	connDone   chan struct{}
	connErr    error
	authDone   chan struct{}
	authUser   U
	udpAccess  sync.RWMutex
	udpConnMap map[uint16]*udpPacketConn
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
	go s.loopMessages()
	go s.handleAuthTimeout()
	go s.loopHeartbeats()
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
				s.closeWithError(E.Cause(err, "handle uni stream"))
			}
		}()
	}
}

func (s *serverSession[U]) handleUniStream(stream quic.ReceiveStream) error {
	defer stream.CancelRead(0)
	buffer := buf.New()
	defer buffer.Release()
	_, err := buffer.ReadAtLeastFrom(stream, 2)
	if err != nil {
		return E.Cause(err, "read request")
	}
	version := buffer.Byte(0)
	if version != Version {
		return E.New("unknown version ", buffer.Byte(0))
	}
	command := buffer.Byte(1)
	switch command {
	case CommandAuthenticate:
		select {
		case <-s.authDone:
			return E.New("authentication: multiple authentication requests")
		default:
		}
		if buffer.Len() < AuthenticateLen {
			_, err = buffer.ReadFullFrom(stream, AuthenticateLen-buffer.Len())
			if err != nil {
				return E.Cause(err, "authentication: read request")
			}
		}
		var userUUID [16]byte
		copy(userUUID[:], buffer.Range(2, 2+16))
		user, loaded := s.userMap[userUUID]
		if !loaded {
			return E.New("authentication: unknown user ", uuid.UUID(userUUID))
		}
		handshakeState := s.quicConn.ConnectionState()
		tuicToken, err := handshakeState.TLS.ExportKeyingMaterial(string(userUUID[:]), []byte(s.passwordMap[user]), 32)
		if err != nil {
			return E.Cause(err, "authentication: export keying material")
		}
		if !bytes.Equal(tuicToken, buffer.Range(2+16, 2+16+32)) {
			return E.New("authentication: token mismatch")
		}
		s.authUser = user
		close(s.authDone)
		return nil
	case CommandPacket:
		select {
		case <-s.connDone:
			return s.connErr
		case <-s.authDone:
		}
		message := allocMessage()
		err = readUDPMessage(message, io.MultiReader(bytes.NewReader(buffer.From(2)), stream))
		if err != nil {
			message.release()
			return err
		}
		s.handleUDPMessage(message, true)
		return nil
	case CommandDissociate:
		select {
		case <-s.connDone:
			return s.connErr
		case <-s.authDone:
		}
		if buffer.Len() > 4 {
			return E.New("invalid dissociate message")
		}
		var sessionID uint16
		err = binary.Read(io.MultiReader(bytes.NewReader(buffer.From(2)), stream), binary.BigEndian, &sessionID)
		if err != nil {
			return err
		}
		s.udpAccess.RLock()
		udpConn, loaded := s.udpConnMap[sessionID]
		s.udpAccess.RUnlock()
		if loaded {
			udpConn.closeWithError(E.New("remote closed"))
			s.udpAccess.Lock()
			delete(s.udpConnMap, sessionID)
			s.udpAccess.Unlock()
		}
		return nil
	default:
		return E.New("unknown command ", command)
	}
}

func (s *serverSession[U]) handleAuthTimeout() {
	select {
	case <-s.connDone:
	case <-s.authDone:
	case <-time.After(s.authTimeout):
		s.closeWithError(E.New("authentication timeout"))
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
				s.logger.Error(E.Cause(err, "handle stream request"))
			}
		}()
	}
}

func (s *serverSession[U]) handleStream(stream quic.Stream) error {
	buffer := buf.NewSize(2 + M.MaxSocksaddrLength)
	defer buffer.Release()
	_, err := buffer.ReadAtLeastFrom(stream, 2)
	if err != nil {
		return E.Cause(err, "read request")
	}
	version, _ := buffer.ReadByte()
	if version != Version {
		return E.New("unknown version ", buffer.Byte(0))
	}
	command, _ := buffer.ReadByte()
	if command != CommandConnect {
		return E.New("unsupported stream command ", command)
	}
	destination, err := AddressSerializer.ReadAddrPort(io.MultiReader(buffer, stream))
	if err != nil {
		return E.Cause(err, "read request destination")
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
	s.handler.NewConnectionEx(auth.ContextWithUser(s.ctx, s.authUser), conn, M.SocksaddrFromNet(s.quicConn.RemoteAddr()).Unwrap(), destination, nil)
	return nil
}

func (s *serverSession[U]) loopHeartbeats() {
	ticker := time.NewTicker(s.heartbeat)
	defer ticker.Stop()
	for {
		select {
		case <-s.connDone:
			return
		case <-ticker.C:
			err := s.quicConn.SendDatagram([]byte{Version, CommandHeartbeat})
			if err != nil {
				s.closeWithError(E.Cause(err, "send heartbeat"))
			}
		}
	}
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
	if E.IsClosedOrCanceled(err) {
		s.logger.Debug(E.Cause(err, "connection failed"))
	} else {
		s.logger.Error(E.Cause(err, "connection failed"))
	}
	_ = s.quicConn.CloseWithError(0, "")
}

type serverConn struct {
	quic.Stream
	destination M.Socksaddr
}

func (c *serverConn) Read(p []byte) (n int, err error) {
	n, err = c.Stream.Read(p)
	return n, baderror.WrapQUIC(err)
}

func (c *serverConn) Write(p []byte) (n int, err error) {
	n, err = c.Stream.Write(p)
	return n, baderror.WrapQUIC(err)
}

func (c *serverConn) LocalAddr() net.Addr {
	return c.destination
}

func (c *serverConn) RemoteAddr() net.Addr {
	return M.Socksaddr{}
}

func (c *serverConn) Close() error {
	c.Stream.CancelRead(0)
	return c.Stream.Close()
}
