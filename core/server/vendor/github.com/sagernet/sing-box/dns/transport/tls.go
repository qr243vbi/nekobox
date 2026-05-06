package transport

import (
	"context"
	"time"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/common/dialer"
	"github.com/sagernet/sing-box/common/tls"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/dns"
	"github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"

	mDNS "github.com/miekg/dns"
)

var _ adapter.DNSTransport = (*TLSTransport)(nil)

func RegisterTLS(registry *dns.TransportRegistry) {
	dns.RegisterTransport[option.RemoteTLSDNSServerOptions](registry, C.DNSTypeTLS, NewTLS)
}

type TLSTransport struct {
	dns.TransportAdapter
	logger logger.ContextLogger

	dialer      tls.Dialer
	serverAddr  M.Socksaddr
	tlsConfig   tls.Config
	connections *ConnPool[*tlsDNSConn]
}

type tlsDNSConn struct {
	tls.Conn
	queryId uint16
}

func NewTLS(ctx context.Context, logger log.ContextLogger, tag string, options option.RemoteTLSDNSServerOptions) (adapter.DNSTransport, error) {
	transportDialer, err := dns.NewRemoteDialer(ctx, options.RemoteDNSServerOptions)
	if err != nil {
		return nil, err
	}
	tlsOptions := common.PtrValueOrDefault(options.TLS)
	tlsOptions.Enabled = true
	tlsConfig, err := tls.NewClient(ctx, logger, options.Server, tlsOptions)
	if err != nil {
		return nil, err
	}
	serverAddr := options.DNSServerAddressOptions.Build()
	if serverAddr.Port == 0 {
		serverAddr.Port = 853
	}
	if !serverAddr.IsValid() {
		return nil, E.New("invalid server address: ", serverAddr)
	}
	return NewTLSRaw(logger, dns.NewTransportAdapterWithRemoteOptions(C.DNSTypeTLS, tag, options.RemoteDNSServerOptions), transportDialer, serverAddr, tlsConfig), nil
}

func NewTLSRaw(logger logger.ContextLogger, adapter dns.TransportAdapter, dialer N.Dialer, serverAddr M.Socksaddr, tlsConfig tls.Config) *TLSTransport {
	return &TLSTransport{
		TransportAdapter: adapter,
		logger:           logger,
		dialer:           tls.NewDialer(dialer, tlsConfig),
		serverAddr:       serverAddr,
		tlsConfig:        tlsConfig,
		connections: NewConnPool(ConnPoolOptions[*tlsDNSConn]{
			Mode: ConnPoolOrdered,
			IsAlive: func(conn *tlsDNSConn) bool {
				return conn != nil
			},
			Close: func(conn *tlsDNSConn, _ error) {
				conn.Close()
			},
		}),
	}
}

func (t *TLSTransport) Start(stage adapter.StartStage) error {
	if stage != adapter.StartStateStart {
		return nil
	}
	return dialer.InitializeDetour(t.dialer)
}

func (t *TLSTransport) Close() error {
	return t.connections.Close()
}

func (t *TLSTransport) Reset() {
	t.connections.Reset()
}

func (t *TLSTransport) Exchange(ctx context.Context, message *mDNS.Msg) (*mDNS.Msg, error) {
	var lastErr error
	for attempt := 0; attempt < 2; attempt++ {
		conn, created, err := t.connections.Acquire(ctx, func(ctx context.Context) (*tlsDNSConn, error) {
			tlsConn, err := t.dialer.DialTLSContext(ctx, t.serverAddr)
			if err != nil {
				return nil, E.Cause(err, "dial TLS connection")
			}
			return &tlsDNSConn{Conn: tlsConn}, nil
		})
		if err != nil {
			return nil, err
		}
		response, err := t.exchange(ctx, message, conn)
		if err == nil {
			t.connections.Release(conn, true)
			return response, nil
		}
		lastErr = err
		t.logger.DebugContext(ctx, "discarded pooled connection: ", err)
		t.connections.Release(conn, false)
		if created {
			return nil, err
		}
	}
	return nil, lastErr
}

func (t *TLSTransport) exchange(ctx context.Context, message *mDNS.Msg, conn *tlsDNSConn) (*mDNS.Msg, error) {
	if deadline, ok := ctx.Deadline(); ok {
		conn.SetDeadline(deadline)
	}
	conn.queryId++
	err := WriteMessage(conn, conn.queryId, message)
	if err != nil {
		return nil, E.Cause(err, "write request")
	}
	response, err := ReadMessage(conn)
	if err != nil {
		return nil, E.Cause(err, "read response")
	}
	conn.SetDeadline(time.Time{})
	return response, nil
}
