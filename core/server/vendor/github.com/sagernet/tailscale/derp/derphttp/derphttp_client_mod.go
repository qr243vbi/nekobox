package derphttp

import (
	"cmp"
	"context"
	"crypto/tls"
	"errors"
	"fmt"
	"github.com/sagernet/tailscale/net/sockstats"
	tailcfg "github.com/sagernet/tailscale/tailcfg"
	"io"
	"net"
	"net/netip"
	"time"
)

func (c *Client) DialRegionTLS(ctx context.Context, reg *tailcfg.DERPRegion) (tlsConn *tls.Conn, connClose io.Closer, node *tailcfg.DERPNode, err error) {
	if len(reg.Nodes) == 0 {
		return nil, nil, nil, fmt.Errorf("no nodes for %s", c.targetString(reg))
	}
	var firstErr error
	for _, n := range reg.Nodes {
		if n.STUNOnly {
			if firstErr == nil {
				firstErr = fmt.Errorf("no non-STUNOnly nodes for %s", c.targetString(reg))
			}
			continue
		}
		c, err := c.dialNodeTLS(ctx, n)
		if err == nil {
			return c, c.NetConn(), n, nil
		}
		if firstErr == nil {
			firstErr = err
		}
	}
	return nil, nil, nil, firstErr
}

func (c *Client) dialNodeTLS(ctx context.Context, n *tailcfg.DERPNode) (*tls.Conn, error) {
	type res struct {
		c   *tls.Conn
		err error
	}
	resc := make(chan res) // must be unbuffered
	ctx, cancel := context.WithTimeout(ctx, dialNodeTimeout)
	defer cancel()

	ctx = sockstats.WithSockStats(ctx, sockstats.LabelDERPHTTPClient, c.logf)

	nwait := 0
	startDial := func(dstPrimary, proto string) {
		nwait++
		go func() {
			if proto == "tcp4" && c.preferIPv6() {
				t, tChannel := c.clock.NewTimer(200 * time.Millisecond)
				select {
				case <-ctx.Done():
					// Either user canceled original context,
					// it timed out, or the v6 dial succeeded.
					t.Stop()
					return
				case <-tChannel:
					// Start v4 dial
				}
			}
			dst := cmp.Or(dstPrimary, n.HostName)
			port := "443"
			if !c.useHTTPS() {
				port = "3340"
			}
			if n.DERPPort != 0 {
				port = fmt.Sprint(n.DERPPort)
			}
			conn, err := c.dialContext(ctx, proto, net.JoinHostPort(dst, port))
			if err != nil {
				select {
				case resc <- res{nil, err}:
				case <-ctx.Done():
				}
				return
			}
			tlsConn := c.tlsClient(conn, n)
			err = tlsConn.Handshake()
			select {
			case resc <- res{tlsConn, err}:
			case <-ctx.Done():
				if c != nil {
					c.Close()
				}
			}
		}()
	}
	if shouldDialProto(n.IPv4, netip.Addr.Is4) {
		startDial(n.IPv4, "tcp4")
	}
	if shouldDialProto(n.IPv6, netip.Addr.Is6) {
		startDial(n.IPv6, "tcp6")
	}
	if nwait == 0 {
		return nil, errors.New("both IPv4 and IPv6 are explicitly disabled for node")
	}

	var firstErr error
	for {
		select {
		case res := <-resc:
			nwait--
			if res.err == nil {
				return res.c, nil
			}
			if firstErr == nil {
				firstErr = res.err
			}
			if nwait == 0 {
				return nil, firstErr
			}
		case <-ctx.Done():
			return nil, ctx.Err()
		}
	}
}
