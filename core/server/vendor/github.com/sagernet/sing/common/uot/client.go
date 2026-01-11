package uot

import (
	"context"
	"net"

	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

type Client struct {
	Dialer  N.Dialer
	Version uint8
}

func (c *Client) DialConn(conn net.Conn, isConnect bool, destination M.Socksaddr) (*Conn, error) {
	switch c.Version {
	case 0, Version:
		request := Request{
			IsConnect:   isConnect,
			Destination: destination,
		}
		err := WriteRequest(conn, request)
		if err != nil {
			return nil, err
		}
		return NewConn(conn, request), nil
	case LegacyVersion:
		return NewConn(conn, Request{}), nil
	default:
		return nil, E.New("unknown protocol version: ", c.Version)
	}
}

func (c *Client) DialEarlyConn(conn net.Conn, isConnect bool, destination M.Socksaddr) (*Conn, error) {
	switch c.Version {
	case 0, Version:
		request := Request{
			IsConnect:   isConnect,
			Destination: destination,
		}
		return NewLazyConn(conn, request), nil
	case LegacyVersion:
		return NewConn(conn, Request{}), nil
	default:
		return nil, E.New("unknown protocol version: ", c.Version)
	}
}

func (c *Client) DialContext(ctx context.Context, network string, destination M.Socksaddr) (net.Conn, error) {
	switch N.NetworkName(network) {
	case N.NetworkUDP:
		tcpConn, err := c.Dialer.DialContext(ctx, N.NetworkTCP, RequestDestination(c.Version))
		if err != nil {
			return nil, err
		}
		uConn, err := c.DialEarlyConn(tcpConn, true, destination)
		if err != nil {
			tcpConn.Close()
			return nil, err
		}
		return uConn, nil
	default:
		return c.Dialer.DialContext(ctx, network, destination)
	}
}

func (c *Client) ListenPacket(ctx context.Context, destination M.Socksaddr) (net.PacketConn, error) {
	tcpConn, err := c.Dialer.DialContext(ctx, N.NetworkTCP, RequestDestination(c.Version))
	if err != nil {
		return nil, err
	}
	uConn, err := c.DialEarlyConn(tcpConn, false, destination)
	if err != nil {
		tcpConn.Close()
		return nil, err
	}
	return uConn, nil
}
