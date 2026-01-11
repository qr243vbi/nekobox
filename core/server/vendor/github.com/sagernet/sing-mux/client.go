package mux

import (
	"context"
	"net"
	"sync"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/x/list"
)

type Client struct {
	dialer         N.Dialer
	logger         logger.Logger
	protocol       byte
	maxConnections int
	minStreams     int
	maxStreams     int
	padding        bool
	access         sync.Mutex
	connections    list.List[abstractSession]
	brutal         BrutalOptions
}

type Options struct {
	Dialer         N.Dialer
	Logger         logger.Logger
	Protocol       string
	MaxConnections int
	MinStreams     int
	MaxStreams     int
	Padding        bool
	Brutal         BrutalOptions
}

type BrutalOptions struct {
	Enabled    bool
	SendBPS    uint64
	ReceiveBPS uint64
}

func NewClient(options Options) (*Client, error) {
	client := &Client{
		dialer:         options.Dialer,
		logger:         options.Logger,
		maxConnections: options.MaxConnections,
		minStreams:     options.MinStreams,
		maxStreams:     options.MaxStreams,
		padding:        options.Padding,
		brutal:         options.Brutal,
	}
	if client.dialer == nil {
		client.dialer = N.SystemDialer
	}
	if client.maxStreams == 0 && client.maxConnections == 0 {
		client.minStreams = 8
	}
	switch options.Protocol {
	case "", "h2mux":
		client.protocol = ProtocolH2Mux
	case "smux":
		client.protocol = ProtocolSmux
	case "yamux":
		client.protocol = ProtocolYAMux
	default:
		return nil, E.New("unknown protocol: " + options.Protocol)
	}
	return client, nil
}

func (c *Client) DialContext(ctx context.Context, network string, destination M.Socksaddr) (net.Conn, error) {
	switch N.NetworkName(network) {
	case N.NetworkTCP:
		stream, err := c.openStream(ctx)
		if err != nil {
			return nil, err
		}
		return &clientConn{Conn: stream, destination: destination}, nil
	case N.NetworkUDP:
		stream, err := c.openStream(ctx)
		if err != nil {
			return nil, err
		}
		extendedConn := bufio.NewExtendedConn(stream)
		return &clientPacketConn{AbstractConn: extendedConn, conn: extendedConn, destination: destination}, nil
	default:
		return nil, E.Extend(N.ErrUnknownNetwork, network)
	}
}

func (c *Client) ListenPacket(ctx context.Context, destination M.Socksaddr) (net.PacketConn, error) {
	stream, err := c.openStream(ctx)
	if err != nil {
		return nil, err
	}
	extendedConn := bufio.NewExtendedConn(stream)
	return &clientPacketAddrConn{AbstractConn: extendedConn, conn: extendedConn, destination: destination}, nil
}

func (c *Client) openStream(ctx context.Context) (net.Conn, error) {
	var (
		session abstractSession
		stream  net.Conn
		err     error
	)
	for attempts := 0; attempts < 2; attempts++ {
		session, err = c.offer(ctx)
		if err != nil {
			continue
		}
		stream, err = session.Open()
		if err != nil {
			continue
		}
		break
	}
	if err != nil {
		return nil, err
	}
	return &wrapStream{stream}, nil
}

func (c *Client) offer(ctx context.Context) (abstractSession, error) {
	c.access.Lock()
	defer c.access.Unlock()

	var sessions []abstractSession
	for element := c.connections.Front(); element != nil; {
		if element.Value.IsClosed() {
			element.Value.Close()
			nextElement := element.Next()
			c.connections.Remove(element)
			element = nextElement
			continue
		}
		sessions = append(sessions, element.Value)
		element = element.Next()
	}
	if c.brutal.Enabled {
		if len(sessions) > 0 {
			return sessions[0], nil
		}
		return c.offerNew(ctx)
	}
	session := common.MinBy(common.Filter(sessions, abstractSession.CanTakeNewRequest), abstractSession.NumStreams)
	if session == nil {
		return c.offerNew(ctx)
	}
	numStreams := session.NumStreams()
	if numStreams == 0 {
		return session, nil
	}
	if c.maxConnections > 0 {
		if len(sessions) >= c.maxConnections || numStreams < c.minStreams {
			return session, nil
		}
	} else {
		if c.maxStreams > 0 && numStreams < c.maxStreams {
			return session, nil
		}
	}
	return c.offerNew(ctx)
}

func (c *Client) offerNew(ctx context.Context) (abstractSession, error) {
	ctx, cancel := context.WithTimeout(ctx, TCPTimeout)
	defer cancel()
	conn, err := c.dialer.DialContext(ctx, N.NetworkTCP, Destination)
	if err != nil {
		return nil, err
	}
	var version byte
	if c.padding {
		version = Version1
	} else {
		version = Version0
	}
	conn = newProtocolConn(conn, Request{
		Version:  version,
		Protocol: c.protocol,
		Padding:  c.padding,
	})
	if c.padding {
		conn = newPaddingConn(conn)
	}
	session, err := newClientSession(conn, c.protocol)
	if err != nil {
		conn.Close()
		return nil, err
	}
	if c.brutal.Enabled {
		err = c.brutalExchange(ctx, conn, session)
		if err != nil {
			conn.Close()
			session.Close()
			return nil, E.Cause(err, "brutal exchange")
		}
	}
	c.connections.PushBack(session)
	return session, nil
}

func (c *Client) brutalExchange(ctx context.Context, sessionConn net.Conn, session abstractSession) error {
	stream, err := session.Open()
	if err != nil {
		return err
	}
	conn := &clientConn{Conn: &wrapStream{stream}, destination: M.Socksaddr{Fqdn: BrutalExchangeDomain}}
	err = WriteBrutalRequest(conn, c.brutal.ReceiveBPS)
	if err != nil {
		return err
	}
	serverReceiveBPS, err := ReadBrutalResponse(conn)
	if err != nil {
		return err
	}
	conn.Close()
	sendBPS := c.brutal.SendBPS
	if serverReceiveBPS < sendBPS {
		sendBPS = serverReceiveBPS
	}
	clientBrutalErr := SetBrutalOptions(sessionConn, sendBPS)
	if clientBrutalErr != nil {
		c.logger.Debug(E.Cause(clientBrutalErr, "failed to enable TCP Brutal at client"))
	}
	return nil
}

func (c *Client) Reset() {
	c.access.Lock()
	defer c.access.Unlock()
	for _, session := range c.connections.Array() {
		session.Close()
	}
	c.connections.Init()
}

func (c *Client) Close() error {
	c.Reset()
	return nil
}
