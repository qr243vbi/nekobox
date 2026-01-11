package hysteria

import (
	"context"
	"errors"
	"io"
	"math"
	"net"
	"os"
	"runtime"
	"sync"
	"time"

	"github.com/sagernet/quic-go"
	"github.com/sagernet/sing-quic"
	hyCC "github.com/sagernet/sing-quic/hysteria/congestion"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/auth"
	"github.com/sagernet/sing/common/baderror"
	"github.com/sagernet/sing/common/canceler"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	aTLS "github.com/sagernet/sing/common/tls"
)

type ServiceOptions struct {
	Context       context.Context
	Logger        logger.Logger
	BrutalDebug   bool
	SendBPS       uint64
	ReceiveBPS    uint64
	XPlusPassword string
	TLSConfig     aTLS.ServerConfig
	UDPDisabled   bool
	UDPTimeout    time.Duration
	Handler       ServerHandler

	// Legacy options

	ConnReceiveWindow   uint64
	StreamReceiveWindow uint64
	MaxIncomingStreams  int64
	DisableMTUDiscovery bool
}

type ServerHandler interface {
	N.TCPConnectionHandlerEx
	N.UDPConnectionHandlerEx
}

type Service[U comparable] struct {
	ctx           context.Context
	logger        logger.Logger
	brutalDebug   bool
	sendBPS       uint64
	receiveBPS    uint64
	xplusPassword string
	tlsConfig     aTLS.ServerConfig
	quicConfig    *quic.Config
	userMap       map[string]U
	udpDisabled   bool
	udpTimeout    time.Duration
	handler       ServerHandler
	quicListener  io.Closer
}

func NewService[U comparable](options ServiceOptions) (*Service[U], error) {
	quicConfig := &quic.Config{
		DisablePathMTUDiscovery:        !(runtime.GOOS == "windows" || runtime.GOOS == "linux" || runtime.GOOS == "android" || runtime.GOOS == "darwin"),
		EnableDatagrams:                !options.UDPDisabled,
		MaxIncomingStreams:             1 << 60,
		InitialStreamReceiveWindow:     DefaultStreamReceiveWindow,
		MaxStreamReceiveWindow:         DefaultStreamReceiveWindow,
		InitialConnectionReceiveWindow: DefaultConnReceiveWindow,
		MaxConnectionReceiveWindow:     DefaultConnReceiveWindow,
		MaxIdleTimeout:                 DefaultMaxIdleTimeout,
		KeepAlivePeriod:                DefaultKeepAlivePeriod,
		DisablePathManager:             true,
	}
	if options.StreamReceiveWindow != 0 {
		quicConfig.InitialStreamReceiveWindow = options.StreamReceiveWindow
		quicConfig.MaxStreamReceiveWindow = options.StreamReceiveWindow
	}
	if options.ConnReceiveWindow != 0 {
		quicConfig.InitialConnectionReceiveWindow = options.ConnReceiveWindow
		quicConfig.MaxConnectionReceiveWindow = options.ConnReceiveWindow
	}
	if options.MaxIncomingStreams > 0 {
		quicConfig.MaxIncomingStreams = int64(options.MaxIncomingStreams)
	}
	if options.DisableMTUDiscovery {
		quicConfig.DisablePathMTUDiscovery = true
	}
	if len(options.TLSConfig.NextProtos()) == 0 {
		options.TLSConfig.SetNextProtos([]string{DefaultALPN})
	}
	if options.SendBPS == 0 {
		return nil, E.New("missing upload speed configuration")
	}
	if options.ReceiveBPS == 0 {
		return nil, E.New("missing download speed configuration")
	}
	return &Service[U]{
		ctx:           options.Context,
		logger:        options.Logger,
		brutalDebug:   options.BrutalDebug,
		sendBPS:       options.SendBPS,
		receiveBPS:    options.ReceiveBPS,
		xplusPassword: options.XPlusPassword,
		tlsConfig:     options.TLSConfig,
		quicConfig:    quicConfig,
		userMap:       make(map[string]U),
		handler:       options.Handler,
		udpDisabled:   options.UDPDisabled,
		udpTimeout:    options.UDPTimeout,
	}, nil
}

func (s *Service[U]) UpdateUsers(userList []U, passwordList []string) {
	userMap := make(map[string]U)
	for i, user := range userList {
		userMap[passwordList[i]] = user
	}
	s.userMap = userMap
}

func (s *Service[U]) Start(conn net.PacketConn) error {
	if s.xplusPassword != "" {
		conn = NewXPlusPacketConn(conn, []byte(s.xplusPassword))
	}
	listener, err := qtls.Listen(conn, s.tlsConfig, s.quicConfig)
	if err != nil {
		return err
	}
	s.quicListener = listener
	go s.loopConnections(listener)
	return nil
}

func (s *Service[U]) Close() error {
	return common.Close(
		s.quicListener,
	)
}

func (s *Service[U]) loopConnections(listener qtls.Listener) {
	for {
		connection, err := listener.Accept(s.ctx)
		if err != nil {
			if E.IsClosedOrCanceled(err) || errors.Is(err, quic.ErrServerClosed) {
				s.logger.Debug(E.Cause(err, "listener closed"))
			} else {
				s.logger.Error(E.Cause(err, "listener closed"))
			}
			return
		}
		session := &serverSession[U]{
			Service:    s,
			ctx:        s.ctx,
			quicConn:   connection,
			connDone:   make(chan struct{}),
			udpConnMap: make(map[uint32]*udpPacketConn),
		}
		go session.handleConnection()
	}
}

type serverSession[U comparable] struct {
	*Service[U]
	ctx          context.Context
	quicConn     quic.Connection
	connAccess   sync.Mutex
	connDone     chan struct{}
	connErr      error
	authUser     U
	udpAccess    sync.RWMutex
	udpConnMap   map[uint32]*udpPacketConn
	udpSessionID uint32
}

func (s *serverSession[U]) handleConnection() {
	ctx, cancel := context.WithTimeout(s.ctx, ProtocolTimeout)
	controlStream, err := s.quicConn.AcceptStream(ctx)
	cancel()
	if err != nil {
		s.closeWithError0(ErrorCodeProtocolError, err)
		return
	}
	clientHello, err := ReadClientHello(controlStream)
	if err != nil {
		s.closeWithError0(ErrorCodeProtocolError, E.Cause(err, "read client hello"))
		return
	}
	user, loaded := s.userMap[clientHello.Auth]
	if !loaded {
		WriteServerHello(controlStream, ServerHello{
			OK:      false,
			Message: "Wrong password",
		})
		s.closeWithError0(ErrorCodeAuthError, E.New("authentication failed, auth_str=", clientHello.Auth))
		return
	}
	err = WriteServerHello(controlStream, ServerHello{
		OK:      true,
		SendBPS: s.sendBPS,
		RecvBPS: s.receiveBPS,
	})
	if err != nil {
		s.closeWithError(err)
		return
	}
	s.authUser = user
	s.quicConn.SetCongestionControl(hyCC.NewBrutalSender(uint64(math.Min(float64(s.sendBPS), float64(clientHello.RecvBPS))), s.brutalDebug, s.logger))
	if !s.udpDisabled {
		go s.loopMessages()
	}
	s.loopStreams()
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
	request, err := ReadClientRequest(stream)
	if err != nil {
		return E.New("read TCP request")
	}
	ctx := auth.ContextWithUser(s.ctx, s.authUser)
	if !request.UDP {
		s.handler.NewConnectionEx(ctx, &serverConn{Stream: stream}, M.SocksaddrFromNet(s.quicConn.RemoteAddr()).Unwrap(), M.ParseSocksaddrHostPort(request.Host, request.Port).Unwrap(), nil)
	} else {
		if s.udpDisabled {
			return WriteServerResponse(stream, ServerResponse{
				OK:      false,
				Message: "UDP disabled by server",
			})
		}
		var sessionID uint32
		udpConn := newUDPPacketConn(ctx, s.quicConn, func() {
			stream.CancelRead(0)
			stream.Close()
			s.udpAccess.Lock()
			delete(s.udpConnMap, sessionID)
			s.udpAccess.Unlock()
		})
		s.udpAccess.Lock()
		s.udpSessionID++
		sessionID = s.udpSessionID
		udpConn.sessionID = sessionID
		s.udpConnMap[sessionID] = udpConn
		s.udpAccess.Unlock()
		err = WriteServerResponse(stream, ServerResponse{
			OK:           true,
			UDPSessionID: sessionID,
		})
		if err != nil {
			udpConn.closeWithError(E.Cause(err, "write server response"))
			return err
		}
		newCtx, newConn := canceler.NewPacketConn(udpConn.ctx, udpConn, s.udpTimeout)
		go s.handler.NewPacketConnectionEx(newCtx, newConn, M.SocksaddrFromNet(s.quicConn.RemoteAddr()).Unwrap(), M.ParseSocksaddrHostPort(request.Host, request.Port).Unwrap(), nil)
		holdBuffer := make([]byte, 1024)
		for {
			_, hErr := stream.Read(holdBuffer)
			if hErr != nil {
				break
			}
		}
		udpConn.closeWithError(E.Cause(net.ErrClosed, "hold stream closed"))
	}
	return nil
}

func (s *serverSession[U]) closeWithError(err error) {
	s.closeWithError0(ErrorCodeGeneric, err)
}

func (s *serverSession[U]) closeWithError0(errorCode int, err error) {
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
	switch errorCode {
	case ErrorCodeProtocolError:
		_ = s.quicConn.CloseWithError(quic.ApplicationErrorCode(errorCode), "protocol error")
	case ErrorCodeAuthError:
		_ = s.quicConn.CloseWithError(quic.ApplicationErrorCode(errorCode), "auth error")
	default:
		_ = s.quicConn.CloseWithError(quic.ApplicationErrorCode(errorCode), "")
	}
}

type serverConn struct {
	quic.Stream
	responseWritten bool
}

func (c *serverConn) HandshakeFailure(err error) error {
	if c.responseWritten {
		return os.ErrInvalid
	}
	c.responseWritten = true
	return WriteServerResponse(c.Stream, ServerResponse{
		OK:      false,
		Message: err.Error(),
	})
}

func (c *serverConn) HandshakeSuccess() error {
	if c.responseWritten {
		return nil
	}
	c.responseWritten = true
	return WriteServerResponse(c.Stream, ServerResponse{
		OK: true,
	})
}

func (c *serverConn) Read(p []byte) (n int, err error) {
	n, err = c.Stream.Read(p)
	return n, baderror.WrapQUIC(err)
}

func (c *serverConn) Write(p []byte) (n int, err error) {
	if !c.responseWritten {
		c.responseWritten = true
		err = WriteServerResponse(c.Stream, ServerResponse{
			OK: true,
		})
		if err != nil {
			return 0, baderror.WrapQUIC(err)
		}
	}
	n, err = c.Stream.Write(p)
	return n, baderror.WrapQUIC(err)
}

func (c *serverConn) LocalAddr() net.Addr {
	return M.Socksaddr{}
}

func (c *serverConn) RemoteAddr() net.Addr {
	return M.Socksaddr{}
}

func (c *serverConn) Close() error {
	c.Stream.CancelRead(0)
	return c.Stream.Close()
}
