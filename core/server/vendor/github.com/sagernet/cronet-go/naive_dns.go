package cronet

// DNS Hijacking Logic for ServerName Resolution
//
// Priority: serverName (SNI) = ServerName config > ServerAddress
//
// A/AAAA queries for serverName:
//   - If ServerAddress is a domain (different from serverName): redirect query to ServerAddress
//   - If ServerAddress is an IP: return synthetic response (mismatched type returns empty SUCCESS)
//
// HTTPS queries for serverName (ECH):
//   - If fixed ECHConfigList exists: return synthetic response immediately
//   - Otherwise: forward query (priority: ECHQueryServerName > serverName)
//   - Always filter ipv4hint/ipv6hint to prevent incorrect IP usage

import (
	"context"
	"encoding/binary"
	"io"
	"net"
	"net/netip"
	"strconv"
	"strings"
	"time"

	"github.com/sagernet/sing/common/bufio"
	"github.com/sagernet/sing/common/logger"
	M "github.com/sagernet/sing/common/metadata"

	mDNS "github.com/miekg/dns"
)

// DNSResolverFunc resolves a DNS request into a DNS response.
//
// The resolver is used by NaiveClient's optional in-process DNS server. The
// returned message should be a response to the request; the implementation
// will normalize the ID and question section as needed.
type DNSResolverFunc func(ctx context.Context, request *mDNS.Msg) (response *mDNS.Msg)

const chromiumDNSUDPMaxSize = 512

func serveDNSPacketConn(ctx context.Context, conn net.PacketConn, resolver DNSResolverFunc) error {
	defer conn.Close()

	if ctx.Done() != nil {
		done := make(chan struct{})
		defer close(done)
		go func() {
			select {
			case <-ctx.Done():
				conn.Close()
			case <-done:
			}
		}()
	}

	buffer := make([]byte, chromiumDNSUDPMaxSize)
	for {
		conn.SetReadDeadline(time.Now().Add(15 * time.Second))

		n, remoteAddress, err := conn.ReadFrom(buffer)
		if err != nil {
			return err
		}

		var request mDNS.Msg
		err = request.Unpack(buffer[:n])
		if err != nil {
			continue
		}

		response := resolver(ctx, &request)
		response = normalizeDNSResponse(&request, response)

		packed, err := response.Pack()
		if err != nil {
			continue
		}
		if len(packed) > chromiumDNSUDPMaxSize {
			truncated := truncatedDNSResponse(&request, response.Rcode)
			packed, err = truncated.Pack()
			if err != nil {
				continue
			}
		}

		var writeConn net.Conn
		if c, ok := conn.(net.Conn); ok {
			writeConn = c
		} else {
			writeConn = bufio.NewBindPacketConn(conn, remoteAddress)
		}
		_, _ = writeConn.Write(packed)
	}
}

func serveDNSStreamConn(ctx context.Context, conn net.Conn, resolver DNSResolverFunc) error {
	defer conn.Close()

	if ctx.Done() != nil {
		done := make(chan struct{})
		defer close(done)
		go func() {
			select {
			case <-ctx.Done():
				conn.Close()
			case <-done:
			}
		}()
	}

	for {
		conn.SetReadDeadline(time.Now().Add(15 * time.Second))

		var queryLength uint16
		err := binary.Read(conn, binary.BigEndian, &queryLength)
		if err != nil {
			return err
		}
		if queryLength == 0 {
			return nil
		}

		query := make([]byte, int(queryLength))
		_, err = io.ReadFull(conn, query)
		if err != nil {
			return err
		}

		var request mDNS.Msg
		err = request.Unpack(query)
		if err != nil {
			continue
		}

		response := resolver(ctx, &request)
		response = normalizeDNSResponse(&request, response)

		packed, err := response.Pack()
		if err != nil {
			continue
		}

		_ = conn.SetWriteDeadline(time.Now().Add(30 * time.Second))
		var lengthPrefix [2]byte
		binary.BigEndian.PutUint16(lengthPrefix[:], uint16(len(packed)))
		if _, err := conn.Write(lengthPrefix[:]); err != nil {
			return err
		}
		if _, err := conn.Write(packed); err != nil {
			return err
		}
	}
}

func normalizeDNSResponse(request *mDNS.Msg, response *mDNS.Msg) *mDNS.Msg {
	if response == nil {
		fallback := new(mDNS.Msg)
		fallback.SetReply(request)
		fallback.Rcode = mDNS.RcodeServerFailure
		return fallback
	}

	response.Id = request.Id
	response.Response = true
	if len(response.Question) == 0 {
		response.Question = request.Question
	}
	return response
}

func truncatedDNSResponse(request *mDNS.Msg, rcode int) *mDNS.Msg {
	response := new(mDNS.Msg)
	response.SetReply(request)
	response.Truncated = true
	response.Rcode = rcode
	return response
}

func wrapDNSResolverWithECH(
	resolver DNSResolverFunc,
	serverName string,
	echQueryServerName string,
	echConfigGetter func() []byte,
	quicEnabled bool,
	l logger.ContextLogger,
) DNSResolverFunc {
	return func(ctx context.Context, request *mDNS.Msg) *mDNS.Msg {
		if len(request.Question) > 0 {
			question := request.Question[0]
			if question.Qtype == mDNS.TypeHTTPS && matchesServerName(question.Name, serverName) {
				echConfig := echConfigGetter()
				if len(echConfig) > 0 {
					var alpn []string
					if quicEnabled {
						alpn = []string{"h3"}
					} else {
						alpn = []string{"h2"}
					}
					l.DebugContext(ctx, "ech config injected, length: ", len(echConfig))
					return injectECHConfig(request, nil, echConfig, alpn)
				}

				var response *mDNS.Msg
				if echQueryServerName != serverName {
					redirectedRequest := request.Copy()
					redirectedRequest.Question[0].Name = rewriteHTTPSQueryName(question.Name, serverName, echQueryServerName)
					response = resolver(ctx, redirectedRequest)
					if response != nil {
						response = response.Copy()
						response.Question = request.Question
						rewriteHTTPSAnswerNames(response, echQueryServerName, serverName)
					}
				} else {
					response = resolver(ctx, request)
					if response != nil {
						response = response.Copy()
					}
				}

				filterIPHintsFromHTTPS(response)
				return response
			}
		}
		return resolver(ctx, request)
	}
}

func rewriteHTTPSQueryName(queryName, fromServer, toServer string) string {
	queryName = strings.TrimSuffix(queryName, ".")
	fromServer = strings.TrimSuffix(fromServer, ".")
	toServer = strings.TrimSuffix(toServer, ".")

	if strings.HasPrefix(queryName, "_") {
		parts := strings.SplitN(queryName, "._https.", 2)
		if len(parts) == 2 && strings.EqualFold(parts[1], fromServer) {
			return parts[0] + "._https." + toServer + "."
		}
	}

	if strings.EqualFold(queryName, fromServer) {
		return toServer + "."
	}

	return queryName + "."
}

func rewriteHTTPSAnswerNames(response *mDNS.Msg, fromServer, toServer string) {
	fromServer = strings.TrimSuffix(fromServer, ".")
	toServer = strings.TrimSuffix(toServer, ".")

	for _, rr := range response.Answer {
		if https, ok := rr.(*mDNS.HTTPS); ok {
			hdrName := strings.TrimSuffix(https.Hdr.Name, ".")
			if strings.EqualFold(hdrName, fromServer) {
				https.Hdr.Name = toServer + "."
			} else if strings.HasPrefix(hdrName, "_") {
				parts := strings.SplitN(hdrName, "._https.", 2)
				if len(parts) == 2 && strings.EqualFold(parts[1], fromServer) {
					https.Hdr.Name = parts[0] + "._https." + toServer + "."
				}
			}
			targetName := strings.TrimSuffix(https.Target, ".")
			if strings.EqualFold(targetName, fromServer) {
				https.Target = toServer + "."
			}
		}
	}
}

func matchesServerName(queryName, serverName string) bool {
	queryName = strings.TrimSuffix(queryName, ".")
	serverName = strings.TrimSuffix(serverName, ".")

	if strings.EqualFold(queryName, serverName) {
		return true
	}

	if strings.HasPrefix(queryName, "_") {
		parts := strings.SplitN(queryName, "._https.", 2)
		if len(parts) == 2 {
			return strings.EqualFold(parts[1], serverName)
		}
	}

	return false
}

func injectECHConfig(request *mDNS.Msg, response *mDNS.Msg, echConfig []byte, alpn []string) *mDNS.Msg {
	if response == nil {
		response = new(mDNS.Msg)
		response.SetReply(request)
	}

	var servicePort uint16
	var hasServicePort bool
	if len(request.Question) > 0 {
		servicePort, hasServicePort = parseHTTPSServicePort(request.Question[0].Name)
	}

	hasHTTPS := false
	for _, rr := range response.Answer {
		if https, ok := rr.(*mDNS.HTTPS); ok {
			hasHTTPS = true
			updateECHInSVCB(&https.SVCB, echConfig)
			if hasServicePort {
				updatePortInSVCB(&https.SVCB, servicePort)
			}
		}
	}

	if !hasHTTPS && len(request.Question) > 0 {
		queryName := request.Question[0].Name
		targetName := queryName
		if strings.HasPrefix(queryName, "_") {
			parts := strings.SplitN(queryName, "._https.", 2)
			if len(parts) == 2 {
				targetName = parts[1]
			}
		}

		https := &mDNS.HTTPS{
			SVCB: mDNS.SVCB{
				Hdr: mDNS.RR_Header{
					Name:   queryName,
					Rrtype: mDNS.TypeHTTPS,
					Class:  mDNS.ClassINET,
					Ttl:    300,
				},
				Priority: 1,
				Target:   targetName,
			},
		}
		if hasServicePort {
			https.Value = append(https.Value, &mDNS.SVCBPort{Port: servicePort})
		}
		https.Value = append(https.Value, &mDNS.SVCBAlpn{Alpn: alpn})
		https.Value = append(https.Value, &mDNS.SVCBECHConfig{ECH: echConfig})
		response.Answer = append(response.Answer, https)
	}

	return response
}

func updateECHInSVCB(svcb *mDNS.SVCB, echConfig []byte) {
	for i, kv := range svcb.Value {
		if _, ok := kv.(*mDNS.SVCBECHConfig); ok {
			svcb.Value[i] = &mDNS.SVCBECHConfig{ECH: echConfig}
			return
		}
	}
	svcb.Value = append(svcb.Value, &mDNS.SVCBECHConfig{ECH: echConfig})
}

func updatePortInSVCB(svcb *mDNS.SVCB, port uint16) {
	for i, kv := range svcb.Value {
		if _, ok := kv.(*mDNS.SVCBPort); ok {
			svcb.Value[i] = &mDNS.SVCBPort{Port: port}
			return
		}
	}
	svcb.Value = append(svcb.Value, &mDNS.SVCBPort{Port: port})
}

func parseHTTPSServicePort(queryName string) (uint16, bool) {
	trimmedName := strings.TrimSuffix(queryName, ".")
	if !strings.HasPrefix(trimmedName, "_") {
		return 0, false
	}
	parts := strings.SplitN(trimmedName, "._https.", 2)
	if len(parts) != 2 {
		return 0, false
	}
	portValue, err := strconv.Atoi(strings.TrimPrefix(parts[0], "_"))
	if err != nil || portValue <= 0 || portValue > 65535 {
		return 0, false
	}
	return uint16(portValue), true
}

func filterIPHintsFromHTTPS(response *mDNS.Msg) {
	if response == nil {
		return
	}
	for _, rr := range response.Answer {
		if https, ok := rr.(*mDNS.HTTPS); ok {
			filterIPHintsFromSVCB(&https.SVCB)
		}
	}
}

func filterIPHintsFromSVCB(svcb *mDNS.SVCB) {
	filtered := svcb.Value[:0]
	for _, kv := range svcb.Value {
		switch kv.(type) {
		case *mDNS.SVCBIPv4Hint, *mDNS.SVCBIPv6Hint:
		default:
			filtered = append(filtered, kv)
		}
	}
	svcb.Value = filtered
}

func wrapDNSResolverForServerRedirect(
	resolver DNSResolverFunc,
	serverName string,
	serverAddress M.Socksaddr,
) DNSResolverFunc {
	return func(ctx context.Context, request *mDNS.Msg) *mDNS.Msg {
		if len(request.Question) == 0 {
			return resolver(ctx, request)
		}

		question := request.Question[0]
		if question.Qtype != mDNS.TypeA && question.Qtype != mDNS.TypeAAAA {
			return resolver(ctx, request)
		}

		queryName := strings.TrimSuffix(question.Name, ".")
		if !strings.EqualFold(queryName, serverName) {
			return resolver(ctx, request)
		}

		if serverAddress.IsIP() {
			return synthesizeAddressResponse(request, serverAddress.Addr)
		}

		redirectedRequest := request.Copy()
		redirectedRequest.Question[0].Name = mDNS.Fqdn(serverAddress.AddrString())

		response := resolver(ctx, redirectedRequest)
		if response != nil {
			response = response.Copy()
			response.Question = request.Question
			rewriteAddressAnswerNames(response, serverAddress.AddrString(), serverName)
		}
		return response
	}
}

func rewriteAddressAnswerNames(response *mDNS.Msg, fromDomain, toDomain string) {
	fromDomain = strings.TrimSuffix(fromDomain, ".")
	toDomain = strings.TrimSuffix(toDomain, ".")
	toFQDN := toDomain + "."

	for _, rr := range response.Answer {
		hdrName := strings.TrimSuffix(rr.Header().Name, ".")
		if strings.EqualFold(hdrName, fromDomain) {
			rr.Header().Name = toFQDN
		}
	}
}

func synthesizeAddressResponse(request *mDNS.Msg, address netip.Addr) *mDNS.Msg {
	response := new(mDNS.Msg)
	response.SetReply(request)

	if len(request.Question) == 0 {
		return response
	}

	question := request.Question[0]
	if question.Qtype == mDNS.TypeA && address.Is4() {
		response.Answer = append(response.Answer, &mDNS.A{
			Hdr: mDNS.RR_Header{
				Name:   question.Name,
				Rrtype: mDNS.TypeA,
				Class:  mDNS.ClassINET,
				Ttl:    300,
			},
			A: address.AsSlice(),
		})
	} else if question.Qtype == mDNS.TypeAAAA && address.Is6() {
		response.Answer = append(response.Answer, &mDNS.AAAA{
			Hdr: mDNS.RR_Header{
				Name:   question.Name,
				Rrtype: mDNS.TypeAAAA,
				Class:  mDNS.ClassINET,
				Ttl:    300,
			},
			AAAA: address.AsSlice(),
		})
	}

	return response
}
