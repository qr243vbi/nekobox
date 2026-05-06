package cronet

import (
	"context"
	"encoding/base64"
	"errors"
	"net"
	"net/url"
	"os"
	"runtime"
	"sync"
	"sync/atomic"


	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	F "github.com/sagernet/sing/common/format"
	"github.com/sagernet/sing/common/logger"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

var _ N.Dialer = (*NaiveClient)(nil)

type QUICCongestionControl string

const (
	QUICCongestionControlDefault QUICCongestionControl = ""
	QUICCongestionControlBBR     QUICCongestionControl = "TBBR"
	QUICCongestionControlBBRv2   QUICCongestionControl = "B2ON"
	QUICCongestionControlCubic   QUICCongestionControl = "QBIC"
	QUICCongestionControlReno    QUICCongestionControl = "RENO"
)

type clientState uint32

const (
	clientStateCreated clientState = iota
	clientStateStarting
	clientStateRunning
	clientStateClosing
	clientStateClosed
)

type NaiveClient struct {
	state                    atomic.Uint32
	ctx                      context.Context
	dialer                   N.Dialer
	logger                   logger.ContextLogger
	serverAddress            M.Socksaddr
	serverName               string
	serverURL                string
	authorization            string
	concurrency              int
	extraHeaders             map[string]string
	receiveWindow            uint64
	trustedRootCertificates  string
	dnsResolver              DNSResolverFunc
	echEnabled               bool
	echConfigList            []byte
	echQueryServerName       string
	echMutex                 sync.RWMutex
	testForceUDPLoopback     bool
	quicEnabled              bool
	quicCongestionControl    QUICCongestionControl
	quicSessionReceiveWindow uint64
	counter                  atomic.Uint64
	started                  chan struct{}
	engine                   Engine
	streamEngine             StreamEngine
	activeConnections        sync.WaitGroup
	proxyWaitGroup           sync.WaitGroup
	proxyCancel              context.CancelFunc
}

type NaiveClientOptions struct {
	Context                  context.Context
	ServerAddress            M.Socksaddr
	ServerName               string
	Username                 string
	Password                 string
	InsecureConcurrency      int
	ReceiveWindow            uint64
	ExtraHeaders             map[string]string
	TrustedRootCertificates  string
	DNSResolver              DNSResolverFunc
	Logger                   logger.ContextLogger
	Dialer                   N.Dialer
	ECHEnabled               bool
	ECHConfigList            []byte
	ECHQueryServerName       string
	TestForceUDPLoopback     bool
	QUIC                     bool
	QUICCongestionControl    QUICCongestionControl
	QUICSessionReceiveWindow uint64
}

func NewNaiveClient(config NaiveClientOptions) (*NaiveClient, error) {
	err := checkLibrary()
	if err != nil {
		return nil, err
	}
	if !config.ServerAddress.IsValid() {
		return nil, E.New("invalid server address")
	}
	if config.DNSResolver == nil {
		return nil, E.New("DNSResolver is required")
	}
	if config.QUIC && config.InsecureConcurrency > 1 {
		return nil, E.New("insecure concurrency is not supported with QUIC")
	}

	serverName := config.ServerName
	if serverName == "" {
		serverName = config.ServerAddress.AddrString()
	}

	serverURL := &url.URL{
		Scheme: "https",
		Host:   net.JoinHostPort(serverName, F.ToString(config.ServerAddress.Port)),
	}

	var authorization string
	if config.Username != "" {
		authorization = "Basic " + base64.StdEncoding.EncodeToString(
			[]byte(config.Username+":"+config.Password))
	}

	concurrency := config.InsecureConcurrency
	if concurrency < 1 {
		concurrency = 1
	}

	ctx := config.Context
	if ctx == nil {
		ctx = context.Background()
	}

	dialer := config.Dialer
	if dialer == nil {
		dialer = N.SystemDialer
	}

	l := config.Logger
	if l == nil {
		l = logger.NOP()
	}

	return &NaiveClient{
		ctx:                      ctx,
		dialer:                   dialer,
		logger:                   l,
		serverAddress:            config.ServerAddress,
		serverName:               serverName,
		serverURL:                serverURL.String(),
		authorization:            authorization,
		extraHeaders:             config.ExtraHeaders,
		concurrency:              concurrency,
		trustedRootCertificates:  config.TrustedRootCertificates,
		dnsResolver:              config.DNSResolver,
		echEnabled:               config.ECHEnabled,
		echConfigList:            config.ECHConfigList,
		echQueryServerName:       config.ECHQueryServerName,
		testForceUDPLoopback:     config.TestForceUDPLoopback,
		quicEnabled:              config.QUIC,
		quicCongestionControl:    config.QUICCongestionControl,
		receiveWindow:            config.ReceiveWindow,
		quicSessionReceiveWindow: config.QUICSessionReceiveWindow,
		started:                  make(chan struct{}),
	}, nil
}

func (c *NaiveClient) Start() error {
	if !c.state.CompareAndSwap(uint32(clientStateCreated), uint32(clientStateStarting)) {
		state := clientState(c.state.Load())
		switch state {
		case clientStateStarting:
			return errors.New("start already in progress")
		case clientStateRunning:
			return errors.New("already started")
		default:
			return net.ErrClosed
		}
	}

	engine := NewEngine()
	var startError error

	defer func() {
		if startError != nil {
			if c.proxyCancel != nil {
				c.proxyCancel()
			}
			if engine.ptr != 0 {
				engine.Shutdown()
				engine.Destroy()
			}
			c.state.Store(uint32(clientStateClosed))
			close(c.started)
		}
	}()

	if c.trustedRootCertificates != "" {
		if !engine.SetTrustedRootCertificates(c.trustedRootCertificates) {
			startError = E.New("failed to set trusted CA certificates")
			return startError
		}
	}

	proxyContext, proxyCancel := context.WithCancel(c.ctx)
	c.proxyCancel = proxyCancel

	dnsServerAddress := M.ParseSocksaddrHostPort("127.0.0.1", 53)
	dnsResolver := c.dnsResolver

	if c.serverName != c.serverAddress.AddrString() {
		dnsResolver = wrapDNSResolverForServerRedirect(dnsResolver, c.serverName, c.serverAddress)
	}

	if c.echEnabled {
		echQueryServerName := c.echQueryServerName
		if echQueryServerName == "" {
			echQueryServerName = c.serverName
		}
		dnsResolver = wrapDNSResolverWithECH(dnsResolver, c.serverName, echQueryServerName, c.getECHConfigList, c.quicEnabled, c.logger)
	}

	engine.SetDialer(func(address string, port uint16) int {
		if address == dnsServerAddress.AddrString() && port == dnsServerAddress.Port {
			fd, conn, err := createSocketPair()
			if err != nil {
				c.logger.ErrorContext(c.ctx, "socket pair failed: ", err)
				return NetErrorConnectionFailed.Code()
			}

			go func() {
				_ = serveDNSStreamConn(proxyContext, conn, dnsResolver)
			}()

			return fd
		}

		destination := M.ParseSocksaddrHostPort(address, port)
		c.logger.DebugContext(c.ctx, "open TCP connection to ", destination)
		conn, err := c.dialer.DialContext(proxyContext, N.NetworkTCP, destination)
		if err != nil {
			c.logger.ErrorContext(c.ctx, "open TCP connection to ", destination, ": ", err)
			return toNetError(err).Code()
		}

		if tcpConn, ok := N.CastReader[*net.TCPConn](conn); ok {
			fd, duplicateError := dupSocketFD(tcpConn)
			if duplicateError == nil {
				conn.Close()
				return fd
			}
		}

		fd, pipeConn, err := createSocketPair()
		if err != nil {
			c.logger.ErrorContext(c.ctx, "socket pair failed: ", err)
			conn.Close()
			return NetErrorConnectionFailed.Code()
		}

		c.proxyWaitGroup.Add(1)
		go func() {
			defer c.proxyWaitGroup.Done()
			bufio.CopyConn(proxyContext, conn, pipeConn)
			conn.Close()
			pipeConn.Close()
		}()

		return fd
	})

	engine.SetUDPDialer(func(address string, port uint16) (fd int, localAddress string, localPort uint16) {
		if address == dnsServerAddress.AddrString() && port == dnsServerAddress.Port {
			fd, conn, err := createPacketSocketPair(c.testForceUDPLoopback)
			if err != nil {
				c.logger.ErrorContext(c.ctx, "socket pair failed: ", err)
				return NetErrorConnectionFailed.Code(), "", 0
			}
			localAddr := M.SocksaddrFromNet(conn.LocalAddr())
			if localAddr.IsValid() {
				localAddress = localAddr.AddrString()
				localPort = localAddr.Port
			}

			go func() {
				_ = serveDNSPacketConn(proxyContext, conn, dnsResolver)
			}()

			return fd, localAddress, localPort
		}

		destination := M.ParseSocksaddrHostPort(address, port)
		c.logger.DebugContext(c.ctx, "open UDP connection to ", destination)
		conn, err := c.dialer.DialContext(proxyContext, N.NetworkUDP, destination)
		if err != nil {
			c.logger.ErrorContext(c.ctx, "open UDP connection to ", destination, ": ", err)
			return toNetError(err).Code(), "", 0
		}

		localAddr := M.SocksaddrFromNet(conn.LocalAddr())
		if localAddr.IsValid() {
			localAddress = localAddr.AddrString()
			localPort = localAddr.Port
		}

		if udpConn, ok := N.CastReader[*net.UDPConn](conn); ok {
			fd, duplicateError := dupSocketFD(udpConn)
			if duplicateError == nil {
				conn.Close()
				return fd, localAddress, localPort
			}
		}

		fd, pipeConn, err := createPacketSocketPair(c.testForceUDPLoopback)
		if err != nil {
			c.logger.ErrorContext(c.ctx, "socket pair failed: ", err)
			conn.Close()
			return NetErrorConnectionFailed.Code(), "", 0
		}

		remoteAddress := M.SocksaddrFromNet(conn.RemoteAddr())
		packetConn := bufio.NewUnbindPacketConn(conn)
		pipePacketConn := bufio.NewUnbindPacketConnWithAddr(pipeConn.(net.Conn), remoteAddress)

		c.proxyWaitGroup.Add(1)
		go func() {
			defer c.proxyWaitGroup.Done()
			_ = bufio.CopyPacketConn(proxyContext, packetConn, pipePacketConn)
		}()

		return fd, localAddress, localPort
	})

	params := NewEngineParams()
	if c.quicEnabled {
		params.SetEnableQuic(true)
	} else {
		params.SetEnableHTTP2(true)
	}

	startError = params.SetAsyncDNS(true)
	if startError != nil {
		return startError
	}
	startError = params.SetDNSServerOverride([]string{dnsServerAddress.String()})
	if startError != nil {
		return startError
	}

	startError = params.SetUseDnsHttpsSvcb(c.echEnabled)
	if startError != nil {
		return startError
	}

	if c.quicEnabled {
		streamReceiveWindow := c.receiveWindow
		if streamReceiveWindow == 0 {
			streamReceiveWindow = 8 * 1024 * 1024
		}
		sessionReceiveWindow := c.quicSessionReceiveWindow
		if sessionReceiveWindow == 0 {
			sessionReceiveWindow = 20 * 1024 * 1024
		}
		startError = params.SetQUICOptions(string(c.quicCongestionControl), streamReceiveWindow, sessionReceiveWindow)
		if startError != nil {
			return startError
		}
	} else {
		if c.quicCongestionControl != "" {
			startError = params.SetQUICOptions(string(c.quicCongestionControl), 0, 0)
			if startError != nil {
				return startError
			}
		}
		receiveWindow := c.receiveWindow
		if receiveWindow == 0 {
			if runtime.GOOS == "ios" {
				receiveWindow = 4 * 1024 * 1024
			} else {
				receiveWindow = 128 * 1024 * 1024
			}
		}
		startError = params.SetHTTP2Options(receiveWindow, receiveWindow/2)
		if startError != nil {
			return startError
		}
	}

	startError = params.SetSocketPoolOptions(2048, 2048, 2040)
	if startError != nil {
		return startError
	}

	result := engine.StartWithParams(params)
	params.Destroy()
	if result != ResultSuccess {
		startError = E.New("failed to start engine: ", int(result))
		return startError
	}

	c.engine = engine
	c.streamEngine = engine.StreamEngine()

	c.state.Store(uint32(clientStateRunning))
	close(c.started)
	return nil
}

func (c *NaiveClient) Engine() Engine {
	if clientState(c.state.Load()) != clientStateRunning {
		return Engine{}
	}
	return c.engine
}

func (c *NaiveClient) DialEarly(ctx context.Context, destination M.Socksaddr) (NaiveConn, error) {
	state := clientState(c.state.Load())
	switch state {
	case clientStateRunning:
	case clientStateClosed, clientStateClosing:
		return nil, net.ErrClosed
	default:
		select {
		case <-c.started:
			if clientState(c.state.Load()) != clientStateRunning {
				return nil, net.ErrClosed
			}
		case <-c.ctx.Done():
			return nil, c.ctx.Err()
		}
	}
	headers := map[string]string{
		"-connect-authority": destination.String(),
		"Padding":            generatePaddingHeader(),
	}
	if c.authorization != "" {
		headers["proxy-authorization"] = c.authorization
	}
	if c.quicEnabled {
		headers["-force-quic"] = "true"
	}
	for key, value := range c.extraHeaders {
		headers[key] = value
	}

	if c.concurrency > 1 {
		concurrencyIndex := int(c.counter.Add(1) % uint64(c.concurrency))
		headers["-network-isolation-key"] = F.ToString("https://pool-", concurrencyIndex, ":443")
	}
	conn := c.streamEngine.CreateConn(ctx, c.logger, true, true)
	err := conn.Start("CONNECT", c.serverURL, headers, 0, false)
	if err != nil {
		return nil, err
	}
	trackedConn := &trackedNaiveConn{
		NaiveConn: NewNaiveConn(ctx, conn, c.logger),
		client:    c,
	}
	c.activeConnections.Add(1)
	conn.bindTrackedNaiveConn(trackedConn)
	return trackedConn, nil
}

func (c *NaiveClient) DialContext(ctx context.Context, network string, destination M.Socksaddr) (net.Conn, error) {
	if N.NetworkName(network) != N.NetworkTCP {
		return nil, os.ErrInvalid
	}
	conn, err := c.DialEarly(ctx, destination)
	if err != nil {
		return nil, err
	}
	err = conn.HandshakeContext(ctx)
	if err != nil {
		conn.Close()
		return nil, err
	}
	return conn, nil
}

func (c *NaiveClient) ListenPacket(ctx context.Context, destination M.Socksaddr) (net.PacketConn, error) {
	return nil, os.ErrInvalid
}

func (c *NaiveClient) Close() error {
	for {
		state := clientState(c.state.Load())
		switch state {
		case clientStateCreated:
			if c.state.CompareAndSwap(uint32(clientStateCreated), uint32(clientStateClosed)) {
				close(c.started)
				return nil
			}

		case clientStateStarting:
			select {
			case <-c.started:
				continue
			case <-c.ctx.Done():
				return c.ctx.Err()
			}

		case clientStateRunning:
			if !c.state.CompareAndSwap(uint32(clientStateRunning), uint32(clientStateClosing)) {
				continue
			}
			return c.doClose()

		case clientStateClosing:
			return nil

		case clientStateClosed:
			return net.ErrClosed
		}
	}
}

func (c *NaiveClient) doClose() error {
	if c.proxyCancel != nil {
		c.proxyCancel()
	}

	c.engine.CloseAllConnections()
	c.proxyWaitGroup.Wait()
	c.activeConnections.Wait()
	c.engine.Shutdown()
	c.engine.Destroy()

	c.state.Store(uint32(clientStateClosed))
	return nil
}

func (c *NaiveClient) getECHConfigList() []byte {
	c.echMutex.RLock()
	defer c.echMutex.RUnlock()
	return c.echConfigList
}

type trackedNaiveConn struct {
	NaiveConn
	client    *NaiveClient
	closeOnce sync.Once
}

func (c *trackedNaiveConn) release() {
	c.closeOnce.Do(func() {
		c.client.activeConnections.Done()
	})
}

func (c *trackedNaiveConn) Close() error {
	c.release()
	return c.NaiveConn.Close()
}
