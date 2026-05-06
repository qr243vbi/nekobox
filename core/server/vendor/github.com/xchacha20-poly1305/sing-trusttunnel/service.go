package trusttunnel

import (
	"context"
	"errors"
	"io"
	"net"
	"net/http"
	"time"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/auth"
	"github.com/sagernet/sing/common/baderror"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/ntp"
	"github.com/sagernet/sing/common/tls"
	sHttp "github.com/sagernet/sing/protocol/http"

	"golang.org/x/net/http2"
	"golang.org/x/net/http2/h2c"
)

type HandlerEx interface {
	N.TCPConnectionHandlerEx
	N.UDPConnectionHandlerEx
}

type ICMPHandler interface {
	NewICMPConnection(ctx context.Context, conn *IcmpConn, onClose N.CloseHandlerFunc)
}

type ServiceOptions struct {
	Ctx                   context.Context
	Logger                logger.ContextLogger
	Handler               HandlerEx
	ICMPHandler           ICMPHandler
	QUICCongestionControl string
}

type Service struct {
	ctx                   context.Context
	logger                logger.ContextLogger
	users                 map[string]string
	handler               HandlerEx
	icmpHandler           ICMPHandler
	quicCongestionControl string
	httpServer            *http.Server
	h2Server              *http2.Server
	h3Server              io.Closer
	tcpListener           net.Listener
	tlsListener           net.Listener
	udpConn               net.PacketConn
	timeFunc              func() time.Time
}

func NewService(options ServiceOptions) *Service {
	timeFunc := ntp.TimeFuncFromContext(options.Ctx)
	if timeFunc == nil {
		timeFunc = time.Now
	}
	return &Service{
		ctx:                   options.Ctx,
		logger:                options.Logger,
		handler:               options.Handler,
		icmpHandler:           options.ICMPHandler,
		quicCongestionControl: options.QUICCongestionControl,
		timeFunc:              timeFunc,
	}
}

type wrapErrorKey struct{}

func contextWithWrapError(ctx context.Context, wrapError func(error) error) context.Context {
	return context.WithValue(ctx, wrapErrorKey{}, wrapError)
}

func wrapErrorFromContext(ctx context.Context) func(error) error {
	return ctx.Value(wrapErrorKey{}).(func(error) error)
}

func (s *Service) Start(tcpListener net.Listener, udpConn net.PacketConn, tlsConfig tls.ServerConfig) error {
	if tcpListener != nil {
		h2Server := &http2.Server{}
		s.httpServer = &http.Server{
			Handler:     h2c.NewHandler(s, h2Server),
			IdleTimeout: DefaultSessionTimeout,
			BaseContext: func(net.Listener) context.Context {
				ctx := s.ctx
				ctx = contextWithWrapError(ctx, baderror.WrapH2)
				return ctx
			},
		}
		err := http2.ConfigureServer(s.httpServer, h2Server)
		if err != nil {
			return err
		}
		s.h2Server = h2Server
		listener := tcpListener
		s.tcpListener = tcpListener
		if tlsConfig != nil {
			listener = tls.NewListener(listener, tlsConfig)
			s.tlsListener = listener
		}
		go func() {
			sErr := s.httpServer.Serve(listener)
			if sErr != nil && !errors.Is(sErr, http.ErrServerClosed) {
				s.logger.ErrorContext(s.ctx, "HTTP server close: ", sErr)
			}
		}()
	}
	if udpConn != nil {
		err := s.configHTTP3Server(tlsConfig, udpConn)
		if err != nil {
			return err
		}
	}
	return nil
}

func (s *Service) UpdateUsers(users []auth.User) {
	userMap := make(map[string]string)
	for _, user := range users {
		userMap[user.Username] = user.Password
	}
	s.users = userMap
}

func (s *Service) Close() error {
	var shutdownErr error
	if s.httpServer != nil {
		const shutdownTimeout = 5 * time.Second
		ctx, cancel := context.WithTimeout(s.ctx, shutdownTimeout)
		shutdownErr = s.httpServer.Shutdown(ctx)
		cancel()
		if errors.Is(shutdownErr, http.ErrServerClosed) {
			shutdownErr = nil
		}
	}
	forceCloseAllH2ServerConnections(s.h2Server)
	closeErr := common.Close(
		common.PtrOrNil(s.httpServer),
		s.tlsListener,
		s.tcpListener,
		s.h3Server,
		s.udpConn,
	)
	return E.Errors(shutdownErr, closeErr)
}

func (s *Service) ServeHTTP(writer http.ResponseWriter, request *http.Request) {
	authorization := request.Header.Get("Proxy-Authorization")
	username, loaded := s.verify(authorization)
	if !loaded {
		writer.WriteHeader(http.StatusProxyAuthRequired)
		s.badRequest(request.Context(), request, E.New("authorization failed"))
		return
	}
	if request.Method != http.MethodConnect {
		writer.WriteHeader(http.StatusMethodNotAllowed)
		s.badRequest(request.Context(), request, E.New("unexpected HTTP method ", request.Method))
		return
	}
	ctx := request.Context()
	ctx = auth.ContextWithUser(ctx, username)
	s.logger.InfoContext(ctx, "[", username, "] ", "request from ", request.RemoteAddr)
	s.logger.InfoContext(ctx, "[", username, "] ", "request to ", request.Host)
	switch request.Host {
	case UDPMagicAddress:
		writer.WriteHeader(http.StatusOK)
		flusher, isFlusher := writer.(http.Flusher)
		if isFlusher {
			flusher.Flush()
		}
		conn := &serverPacketConn{
			packetConn: packetConn{
				httpConn: httpConn{
					writer:    writer,
					flusher:   flusher,
					wrapError: wrapErrorFromContext(ctx),
					created:   make(chan struct{}),
				},
			},
		}
		conn.setUp(request.Body, nil)
		firstPacket := buf.NewPacket()
		destination, err := conn.ReadPacket(firstPacket)
		if err != nil {
			firstPacket.Release()
			_ = conn.Close()
			s.logger.ErrorContext(ctx, E.Cause(err, "read first packet of ", request.RemoteAddr))
			return
		}
		destination = destination.Unwrap()
		cachedConn := bufio.NewCachedPacketConn(conn, firstPacket, destination)
		done := make(chan struct{})
		s.handler.NewPacketConnectionEx(ctx, cachedConn, M.ParseSocksaddr(request.RemoteAddr), destination, N.OnceClose(func(it error) {
			close(done)
		}))
		<-done
	case ICMPMagicAddress:
		flusher, isFlusher := writer.(http.Flusher)
		if s.icmpHandler == nil {
			writer.WriteHeader(http.StatusNotImplemented)
			if isFlusher {
				flusher.Flush()
			}
			_ = request.Body.Close()
		} else {
			writer.WriteHeader(http.StatusOK)
			if isFlusher {
				flusher.Flush()
			}
			conn := &IcmpConn{
				httpConn{
					writer:    writer,
					flusher:   flusher,
					wrapError: wrapErrorFromContext(ctx),
					created:   make(chan struct{}),
				},
			}
			conn.setUp(request.Body, nil)
			done := make(chan struct{})
			s.icmpHandler.NewICMPConnection(ctx, conn, N.OnceClose(func(it error) {
				close(done)
			}))
			<-done
		}
	case HealthCheckMagicAddress:
		writer.WriteHeader(http.StatusOK)
		if flusher, isFlusher := writer.(http.Flusher); isFlusher {
			flusher.Flush()
		}
		_ = request.Body.Close()
	default:
		writer.WriteHeader(http.StatusOK)
		flusher, isFlusher := writer.(http.Flusher)
		if isFlusher {
			flusher.Flush()
		}
		conn := &tcpConn{
			httpConn{
				writer:    writer,
				flusher:   flusher,
				wrapError: wrapErrorFromContext(ctx),
				created:   make(chan struct{}),
			},
		}
		conn.setUp(request.Body, nil)
		done := make(chan struct{})
		s.handler.NewConnectionEx(ctx, conn, M.ParseSocksaddr(request.RemoteAddr), M.ParseSocksaddr(request.Host).Unwrap(), N.OnceClose(func(it error) {
			close(done)
		}))
		<-done
	}
}

func (s *Service) verify(authorization string) (username string, loaded bool) {
	username, password, loaded := sHttp.ParseBasicAuth(authorization)
	if !loaded {
		return "", false
	}
	recordedPassword, loaded := s.users[username]
	if !loaded {
		return "", false
	}
	if password != recordedPassword {
		return "", false
	}
	return username, true
}

func (s *Service) badRequest(ctx context.Context, request *http.Request, err error) {
	s.logger.ErrorContext(ctx, E.Cause(err, "process connection from ", request.RemoteAddr))
}
