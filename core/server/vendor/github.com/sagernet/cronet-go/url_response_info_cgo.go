//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import "unsafe"

func NewURLResponseInfo() URLResponseInfo {
	return URLResponseInfo{uintptr(unsafe.Pointer(C.Cronet_UrlResponseInfo_Create()))}
}

func (i URLResponseInfo) Destroy() {
	C.Cronet_UrlResponseInfo_Destroy(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr)))
}

// The URL the response is for. This is the URL after following
// redirects, so it may not be the originally requested URL
func (i URLResponseInfo) URL() string {
	return C.GoString(C.Cronet_UrlResponseInfo_url_get(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr))))
}

// URLChainSize The URL chain. The first entry is the originally requested URL;
// the following entries are redirects followed.
func (i URLResponseInfo) URLChainSize() int {
	return int(C.Cronet_UrlResponseInfo_url_chain_size(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr))))
}

func (i URLResponseInfo) URLChainAt(index int) string {
	return C.GoString(C.Cronet_UrlResponseInfo_url_chain_at(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr)), C.uint32_t(index)))
}

// StatusCode is the HTTP status code. When a resource is retrieved from the cache,
// whether it was revalidated or not, the original status code is returned.
func (i URLResponseInfo) StatusCode() int {
	return int(C.Cronet_UrlResponseInfo_http_status_code_get(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr))))
}

// StatusText is the HTTP status text of the status line. For example, if the
// request received a "HTTP/1.1 200 OK" response, this method returns "OK".
func (i URLResponseInfo) StatusText() string {
	return C.GoString(C.Cronet_UrlResponseInfo_http_status_text_get(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr))))
}

// HeaderSize list size of response header field and value pairs.
// The headers are in the same order they are received over the wire.
func (i URLResponseInfo) HeaderSize() int {
	return int(C.Cronet_UrlResponseInfo_all_headers_list_size(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr))))
}

func (i URLResponseInfo) HeaderAt(index int) HTTPHeader {
	return HTTPHeader{uintptr(unsafe.Pointer(C.Cronet_UrlResponseInfo_all_headers_list_at(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr)), C.uint32_t(index))))}
}

// Cached true if the response came from the cache, including
// requests that were revalidated over the network before being retrieved
// from the cache, failed otherwise.
func (i URLResponseInfo) Cached() bool {
	return bool(C.Cronet_UrlResponseInfo_was_cached_get(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr))))
}

// NegotiatedProtocol is the protocol (for example 'quic/1+spdy/3') negotiated with the server.
// An empty string if no protocol was negotiated, the protocol is
// not known, or when using plain HTTP or HTTPS.
func (i URLResponseInfo) NegotiatedProtocol() string {
	return C.GoString(C.Cronet_UrlResponseInfo_negotiated_protocol_get(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr))))
}

// ProxyServer is the proxy server that was used for the request.
func (i URLResponseInfo) ProxyServer() string {
	return C.GoString(C.Cronet_UrlResponseInfo_proxy_server_get(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr))))
}

// ReceivedByteCount is a minimum count of bytes received from the network to process this
// request. This count may ignore certain overheads (for example IP and
// TCP/UDP framing, SSL handshake and framing, proxy handling). This count is
// taken prior to decompression (for example GZIP and Brotli) and includes
// headers and data from all redirects.
func (i URLResponseInfo) ReceivedByteCount() int64 {
	return int64(C.Cronet_UrlResponseInfo_received_byte_count_get(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr))))
}

func (i URLResponseInfo) SetURL(url string) {
	cURL := C.CString(url)
	C.Cronet_UrlResponseInfo_url_set(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr)), cURL)
	C.free(unsafe.Pointer(cURL))
}

func (i URLResponseInfo) AddURLChain(url string) {
	cURL := C.CString(url)
	C.Cronet_UrlResponseInfo_url_chain_add(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr)), cURL)
	C.free(unsafe.Pointer(cURL))
}

func (i URLResponseInfo) ClearURLChain() {
	C.Cronet_UrlResponseInfo_url_chain_clear(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr)))
}

func (i URLResponseInfo) SetStatusCode(code int32) {
	C.Cronet_UrlResponseInfo_http_status_code_set(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr)), C.int32_t(code))
}

func (i URLResponseInfo) SetStatusText(text string) {
	cText := C.CString(text)
	C.Cronet_UrlResponseInfo_http_status_text_set(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr)), cText)
	C.free(unsafe.Pointer(cText))
}

func (i URLResponseInfo) AddHeader(header HTTPHeader) {
	C.Cronet_UrlResponseInfo_all_headers_list_add(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr)), C.Cronet_HttpHeaderPtr(unsafe.Pointer(header.ptr)))
}

func (i URLResponseInfo) ClearHeaders() {
	C.Cronet_UrlResponseInfo_all_headers_list_clear(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr)))
}

func (i URLResponseInfo) SetCached(cached bool) {
	C.Cronet_UrlResponseInfo_was_cached_set(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr)), C.bool(cached))
}

func (i URLResponseInfo) SetNegotiatedProtocol(protocol string) {
	cProtocol := C.CString(protocol)
	C.Cronet_UrlResponseInfo_negotiated_protocol_set(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr)), cProtocol)
	C.free(unsafe.Pointer(cProtocol))
}

func (i URLResponseInfo) SetProxyServer(proxy string) {
	cProxy := C.CString(proxy)
	C.Cronet_UrlResponseInfo_proxy_server_set(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr)), cProxy)
	C.free(unsafe.Pointer(cProxy))
}

func (i URLResponseInfo) SetReceivedByteCount(count int64) {
	C.Cronet_UrlResponseInfo_received_byte_count_set(C.Cronet_UrlResponseInfoPtr(unsafe.Pointer(i.ptr)), C.int64_t(count))
}
