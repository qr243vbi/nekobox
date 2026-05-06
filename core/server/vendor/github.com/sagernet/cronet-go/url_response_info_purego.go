//go:build with_purego

package cronet

import (
	"github.com/sagernet/cronet-go/internal/cronet"
)

func NewURLResponseInfo() URLResponseInfo {
	return URLResponseInfo{cronet.UrlResponseInfoCreate()}
}

func (i URLResponseInfo) Destroy() {
	cronet.UrlResponseInfoDestroy(i.ptr)
}

func (i URLResponseInfo) URL() string {
	return cronet.UrlResponseInfoUrlGet(i.ptr)
}

func (i URLResponseInfo) URLChainSize() int {
	return int(cronet.UrlResponseInfoUrlChainSize(i.ptr))
}

func (i URLResponseInfo) URLChainAt(index int) string {
	return cronet.UrlResponseInfoUrlChainAt(i.ptr, uint32(index))
}

func (i URLResponseInfo) StatusCode() int {
	return int(cronet.UrlResponseInfoHttpStatusCodeGet(i.ptr))
}

func (i URLResponseInfo) StatusText() string {
	return cronet.UrlResponseInfoHttpStatusTextGet(i.ptr)
}

func (i URLResponseInfo) HeaderSize() int {
	return int(cronet.UrlResponseInfoAllHeadersListSize(i.ptr))
}

func (i URLResponseInfo) HeaderAt(index int) HTTPHeader {
	return HTTPHeader{cronet.UrlResponseInfoAllHeadersListAt(i.ptr, uint32(index))}
}

func (i URLResponseInfo) Cached() bool {
	return cronet.UrlResponseInfoWasCachedGet(i.ptr)
}

func (i URLResponseInfo) NegotiatedProtocol() string {
	return cronet.UrlResponseInfoNegotiatedProtocolGet(i.ptr)
}

func (i URLResponseInfo) ProxyServer() string {
	return cronet.UrlResponseInfoProxyServerGet(i.ptr)
}

func (i URLResponseInfo) ReceivedByteCount() int64 {
	return cronet.UrlResponseInfoReceivedByteCountGet(i.ptr)
}
