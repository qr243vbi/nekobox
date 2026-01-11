package mux

import (
	"context"
	"net"

	"github.com/sagernet/sing/common/bufio"
	"github.com/sagernet/sing/common/debug"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/task"
)

// Deprecated: Use ServiceHandlerEx instead.
//
//nolint:staticcheck
type ServiceHandler interface {
	N.TCPConnectionHandler
	N.UDPConnectionHandler
}

type ServiceHandlerEx interface {
	N.TCPConnectionHandlerEx
	N.UDPConnectionHandlerEx
}

type Service struct {
	newStreamContext func(context.Context, net.Conn) context.Context
	logger           logger.ContextLogger
	handler          ServiceHandler
	handlerEx        ServiceHandlerEx
	padding          bool
	brutal           BrutalOptions
}

type ServiceOptions struct {
	NewStreamContext func(context.Context, net.Conn) context.Context
	Logger           logger.ContextLogger
	Handler          ServiceHandler
	HandlerEx        ServiceHandlerEx
	Padding          bool
	Brutal           BrutalOptions
}

func NewService(options ServiceOptions) (*Service, error) {
	if options.Brutal.Enabled && !BrutalAvailable && !debug.Enabled {
		return nil, E.New("TCP Brutal is only supported on Linux")
	}
	return &Service{
		newStreamContext: options.NewStreamContext,
		logger:           options.Logger,
		handler:          options.Handler,
		handlerEx:        options.HandlerEx,
		padding:          options.Padding,
		brutal:           options.Brutal,
	}, nil
}

// Deprecated: Use NewConnectionEx instead.
func (s *Service) NewConnection(ctx context.Context, conn net.Conn, metadata M.Metadata) error {
	return s.newConnection(ctx, conn, metadata.Source)
}

func (s *Service) NewConnectionEx(ctx context.Context, conn net.Conn, source M.Socksaddr, destination M.Socksaddr, onClose N.CloseHandlerFunc) {
	err := s.newConnection(ctx, conn, source)
	N.CloseOnHandshakeFailure(conn, onClose, err)
	if err != nil {
		s.logger.ErrorContext(ctx, E.Cause(err, "process multiplex connection from ", source))
	}
}

func (s *Service) newConnection(ctx context.Context, conn net.Conn, source M.Socksaddr) error {
	request, err := ReadRequest(conn)
	if err != nil {
		return err
	}
	if request.Padding {
		conn = newPaddingConn(conn)
	} else if s.padding {
		return E.New("non-padded connection rejected")
	}
	session, err := newServerSession(conn, request.Protocol)
	if err != nil {
		return err
	}
	var group task.Group
	group.Append0(func(_ context.Context) error {
		for {
			stream, aErr := session.Accept()
			if aErr != nil {
				return aErr
			}
			streamCtx := s.newStreamContext(ctx, stream)
			go func() {
				hErr := s.newSession(streamCtx, conn, stream, source)
				if hErr != nil {
					stream.Close()
					s.logger.ErrorContext(streamCtx, E.Cause(hErr, "process multiplex stream"))
				}
			}()
		}
	})
	group.Cleanup(func() {
		session.Close()
	})
	return group.Run(ctx)
}

func (s *Service) newSession(ctx context.Context, sessionConn net.Conn, stream net.Conn, source M.Socksaddr) error {
	stream = &wrapStream{stream}
	request, err := ReadStreamRequest(stream)
	if err != nil {
		return E.Cause(err, "read multiplex stream request")
	}
	destination := request.Destination
	if request.Network == N.NetworkTCP {
		conn := &serverConn{ExtendedConn: bufio.NewExtendedConn(stream)}
		if request.Destination.Fqdn == BrutalExchangeDomain {
			defer stream.Close()
			var clientReceiveBPS uint64
			clientReceiveBPS, err = ReadBrutalRequest(conn)
			if err != nil {
				return E.Cause(err, "read brutal request")
			}
			if !s.brutal.Enabled {
				err = WriteBrutalResponse(conn, 0, false, "brutal is not enabled by the server")
				if err != nil {
					return E.Cause(err, "write brutal response")
				}
				return nil
			}
			sendBPS := s.brutal.SendBPS
			if clientReceiveBPS < sendBPS {
				sendBPS = clientReceiveBPS
			}
			err = SetBrutalOptions(sessionConn, sendBPS)
			if err != nil {
				// ignore error in test
				if !debug.Enabled {
					err = WriteBrutalResponse(conn, 0, false, E.Cause(err, "enable TCP Brutal").Error())
					if err != nil {
						return E.Cause(err, "write brutal response")
					}
					return nil
				}
			}
			err = WriteBrutalResponse(conn, s.brutal.ReceiveBPS, true, "")
			if err != nil {
				return E.Cause(err, "write brutal response")
			}
			return nil
		}
		s.logger.InfoContext(ctx, "inbound multiplex connection to ", destination)
		if s.handler != nil {
			//nolint:staticcheck
			s.handler.NewConnection(ctx, conn, M.Metadata{Source: source, Destination: destination})
		} else {
			s.handlerEx.NewConnectionEx(ctx, conn, source, destination, nil)
		}
	} else {
		var packetConn N.PacketConn
		if !request.PacketAddr {
			s.logger.InfoContext(ctx, "inbound multiplex packet connection to ", destination)
			packetConn = &serverPacketConn{ExtendedConn: bufio.NewExtendedConn(stream), destination: request.Destination}
		} else {
			s.logger.InfoContext(ctx, "inbound multiplex packet connection")
			packetConn = &serverPacketAddrConn{ExtendedConn: bufio.NewExtendedConn(stream)}
		}
		if s.handler != nil {
			//nolint:staticcheck
			s.handler.NewPacketConnection(ctx, packetConn, M.Metadata{Source: source, Destination: destination})
		} else {
			s.handlerEx.NewPacketConnectionEx(ctx, packetConn, source, destination, nil)
		}
	}
	return nil
}
