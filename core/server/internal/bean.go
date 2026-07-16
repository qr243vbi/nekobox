package internal

import (
	"fmt"
	"nekobox_core/gen"
	"net/netip"

	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/common/json/badoption"
)

const BeanOriginal = "B"
const BeanModified = "O"

func ConvertHeaders(lsit map[string]*gen.StringList) badoption.HTTPHeader {
	headers := badoption.HTTPHeader{}
	for dish, price := range lsit {
		headers[dish] = price.GetItems()
	}
	return headers
}

func SetTls(Any *option.OutboundTLSOptions, tls *gen.Outbound_Tls)

func DetourId(Id *gen.Id) string {
	if Id == nil {
		return ""
	}
	return BeanOriginal + fmt.Sprint(Id.Id)
}

func to4ByteArray(b []byte) [4]byte {
	var a [4]byte
	copy(a[:], b)
	return a
}

func to16ByteArray(b []byte) [16]byte {
	var a [16]byte
	copy(a[:], b)
	return a
}

func ConvertAddr(addr *gen.IpAddr) *badoption.Addr {
	if addr == nil {
		return nil
	}
	ret := new(badoption.Addr)
	if addr.GetIfIp4() {
		*ret = badoption.Addr(netip.AddrFrom4(to4ByteArray(addr.GetAddr())))
	} else {
		*ret = badoption.Addr(netip.AddrFrom16(to16ByteArray(addr.GetAddr())))
	}
	return ret
}

func SetDialer(Any *option.DialerOptions, dialer *gen.DialFields) {
	if dialer == nil {
		return
	}
	Any.Detour = DetourId(dialer.GetDetourId())
	Any.Inet4BindAddress = ConvertAddr(dialer.GetInet4BindAddress())
	Any.Inet6BindAddress = ConvertAddr(dialer.GetInet6BindAddress())
}

func SetServerAndPort(Any *option.ServerOptions, addr *gen.ServerAndPort) {
	if addr == nil {
		return
	}
	Any.Server = (addr.GetServer())
	Any.ServerPort = uint16(addr.GetPort())
}

func HttpToOutbound(http *gen.HttpBean, dial *gen.DialFields, addr *gen.ServerAndPort) option.HTTPOutboundOptions {
	ret := option.HTTPOutboundOptions{}
	if http == nil {
		return ret
	}
	ret.Username = http.GetCredentials().GetUsername()
	ret.Password = http.GetCredentials().GetPassword()
	ret.Path = http.GetPath()
	ret.Headers = ConvertHeaders(http.GetHeaders())
	SetServerAndPort(&ret.ServerOptions, addr)
	SetDialer(&ret.DialerOptions, dial)
	SetTls(ret.OutboundTLSOptionsContainer.TLS, http.GetTls())
	return ret
}

func BeanToOutbound(bean *gen.Bean) option.Outbound {
	var outbound option.Outbound
	if bean == nil {
		return outbound
	}
	switch bean_body := bean.GetBean().(type) {
	case *gen.Bean_Http:
		outbound.Options = HttpToOutbound(bean_body.Http, bean.GetDialFields(), bean.GetAddress())
		outbound.Type = "http"
	default:
	}

	outbound.Tag = DetourId(bean.Id)

	return outbound

}
