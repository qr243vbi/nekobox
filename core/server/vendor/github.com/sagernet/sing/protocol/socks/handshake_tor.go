package socks

import (
	"context"
	"net"
	"net/netip"
	"os"
	"strings"

	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	"github.com/sagernet/sing/protocol/socks/socks4"
	"github.com/sagernet/sing/protocol/socks/socks5"
)

const (
	CommandTorResolve    byte = 0xF0
	CommandTorResolvePTR byte = 0xF1
)

type TorResolver interface {
	LookupIP(ctx context.Context, host string) (netip.Addr, error)
	LookupPTR(ctx context.Context, addr netip.Addr) (string, error)
}

func handleTorSocks4(ctx context.Context, conn net.Conn, request socks4.Request, resolver TorResolver) error {
	switch request.Command {
	case CommandTorResolve:
		if !request.Destination.IsFqdn() {
			return E.New("socks4: torsocks: invalid destination")
		}
		ipAddr, err := resolver.LookupIP(ctx, request.Destination.Fqdn)
		if err != nil {
			err = socks4.WriteResponse(conn, socks4.Response{
				ReplyCode: socks4.ReplyCodeRejectedOrFailed,
			})
			if err != nil {
				return err
			}
			return E.Cause(err, "socks4: torsocks: lookup failed for domain: ", request.Destination.Fqdn)
		}
		err = socks4.WriteResponse(conn, socks4.Response{
			ReplyCode:   socks4.ReplyCodeGranted,
			Destination: M.SocksaddrFrom(ipAddr, 0),
		})
		if err != nil {
			return E.Cause(err, "socks4: torsocks: write response")
		}
		return nil
	case CommandTorResolvePTR:
		var ipAddr netip.Addr
		if request.Destination.IsIP() {
			ipAddr = request.Destination.Addr
		} else if strings.HasSuffix(request.Destination.Fqdn, ".in-addr.arpa") {
			ipAddr, _ = netip.ParseAddr(request.Destination.Fqdn[:len(request.Destination.Fqdn)-len(".in-addr.arpa")])
		} else if strings.HasSuffix(request.Destination.Fqdn, ".ip6.arpa") {
			ipAddr, _ = netip.ParseAddr(strings.ReplaceAll(request.Destination.Fqdn[:len(request.Destination.Fqdn)-len(".ip6.arpa")], ".", ":"))
		}
		if !ipAddr.IsValid() {
			return E.New("socks4: torsocks: invalid destination")
		}
		host, err := resolver.LookupPTR(ctx, ipAddr)
		if err != nil {
			err = socks4.WriteResponse(conn, socks4.Response{
				ReplyCode: socks4.ReplyCodeRejectedOrFailed,
			})
			if err != nil {
				return err
			}
			return E.Cause(err, "socks4: torsocks: lookup PTR failed for ip: ", ipAddr)
		}
		err = socks4.WriteResponse(conn, socks4.Response{
			ReplyCode: socks4.ReplyCodeGranted,
			Destination: M.Socksaddr{
				Fqdn: host,
			},
		})
		if err != nil {
			return E.Cause(err, "socks4: torsocks: write response")
		}
		return nil
	default:
		return os.ErrInvalid
	}
}

func handleTorSocks5(ctx context.Context, conn net.Conn, request socks5.Request, resolver TorResolver) error {
	switch request.Command {
	case CommandTorResolve:
		if !request.Destination.IsFqdn() {
			return E.New("socks5: torsocks: invalid destination")
		}
		ipAddr, err := resolver.LookupIP(ctx, request.Destination.Fqdn)
		if err != nil {
			err = socks5.WriteResponse(conn, socks5.Response{
				ReplyCode: socks5.ReplyCodeFailure,
			})
			if err != nil {
				return err
			}
			return E.Cause(err, "socks5: torsocks: lookup failed for domain: ", request.Destination.Fqdn)
		}
		err = socks5.WriteResponse(conn, socks5.Response{
			ReplyCode: socks5.ReplyCodeSuccess,
			Bind:      M.SocksaddrFrom(ipAddr, 0),
		})
		if err != nil {
			return E.Cause(err, "socks5: torsocks: write response")
		}
		return nil
	case CommandTorResolvePTR:
		var ipAddr netip.Addr
		if request.Destination.IsIP() {
			ipAddr = request.Destination.Addr
		} else if strings.HasSuffix(request.Destination.Fqdn, ".in-addr.arpa") {
			ipAddr, _ = netip.ParseAddr(request.Destination.Fqdn[:len(request.Destination.Fqdn)-len(".in-addr.arpa")])
		} else if strings.HasSuffix(request.Destination.Fqdn, ".ip6.arpa") {
			ipAddr, _ = netip.ParseAddr(strings.ReplaceAll(request.Destination.Fqdn[:len(request.Destination.Fqdn)-len(".ip6.arpa")], ".", ":"))
		}
		if !ipAddr.IsValid() {
			return E.New("socks5: torsocks: invalid destination")
		}
		host, err := resolver.LookupPTR(ctx, ipAddr)
		if err != nil {
			err = socks5.WriteResponse(conn, socks5.Response{
				ReplyCode: socks5.ReplyCodeFailure,
			})
			if err != nil {
				return err
			}
			return E.Cause(err, "socks5: torsocks: lookup PTR failed for ip: ", ipAddr)
		}
		err = socks5.WriteResponse(conn, socks5.Response{
			ReplyCode: socks5.ReplyCodeSuccess,
			Bind: M.Socksaddr{
				Fqdn: host,
			},
		})
		if err != nil {
			return E.Cause(err, "socks5: torsocks: write response")
		}
		return nil
	default:
		return os.ErrInvalid
	}
}
