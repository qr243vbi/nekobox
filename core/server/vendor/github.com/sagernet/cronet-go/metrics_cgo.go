//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import "unsafe"

func NewMetrics() Metrics {
	return Metrics{uintptr(unsafe.Pointer(C.Cronet_Metrics_Create()))}
}

func (m Metrics) Destroy() {
	C.Cronet_Metrics_Destroy(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)))
}

// RequestStart
// Time when the request started, which corresponds to calling
// Cronet_UrlRequest_Start(). This timestamp will match the system clock at
// the time it represents.
func (m Metrics) RequestStart() DateTime {
	return DateTime{uintptr(unsafe.Pointer(C.Cronet_Metrics_request_start_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)))))}
}

// DNSStart
// Time when DNS lookup started. This and DNSEnd will be set to
// non-null regardless of whether the result came from a DNS server or the
// local cache. Will equal null if the socket was reused (see SocketReused).
func (m Metrics) DNSStart() DateTime {
	return DateTime{uintptr(unsafe.Pointer(C.Cronet_Metrics_dns_start_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)))))}
}

// DNSEnd
// Time when DNS lookup finished. This and DNSStart will return
// non-null regardless of whether the result came from a DNS server or the
// local cache. Will equal null if the socket was reused (see SocketReused).
func (m Metrics) DNSEnd() DateTime {
	return DateTime{uintptr(unsafe.Pointer(C.Cronet_Metrics_dns_end_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)))))}
}

// ConnectStart
// Time when connection establishment started, typically when DNS resolution
// finishes. Will equal null if the socket was reused (see SocketReused).
func (m Metrics) ConnectStart() DateTime {
	return DateTime{uintptr(unsafe.Pointer(C.Cronet_Metrics_connect_start_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)))))}
}

// ConnectEnd
// Time when connection establishment finished, after TCP connection is
// established and, if using HTTPS, SSL handshake is completed. For QUIC
// 0-RTT, this represents the time of handshake confirmation and might happen
// later than SendingStart. Will equal null if the socket was
// reused (see SocketReused).
func (m Metrics) ConnectEnd() DateTime {
	return DateTime{uintptr(unsafe.Pointer(C.Cronet_Metrics_connect_end_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)))))}
}

// SSLStart
// Time when SSL handshake started. For QUIC, this will be the same time as
// ConnectStart. Will equal null if SSL is not used or if the
// socket was reused (see SocketReused).
func (m Metrics) SSLStart() DateTime {
	return DateTime{uintptr(unsafe.Pointer(C.Cronet_Metrics_ssl_start_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)))))}
}

// SSLEnd
// Time when SSL handshake finished. For QUIC, this will be the same time as
// ConnectEnd. Will equal null if SSL is not used or if the socket
// was reused (see SocketReused).
func (m Metrics) SSLEnd() DateTime {
	return DateTime{uintptr(unsafe.Pointer(C.Cronet_Metrics_ssl_end_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)))))}
}

// SendingStart
// Time when sending HTTP request headers started.
//
// Will equal null if the request failed or was canceled before sending
// started.
func (m Metrics) SendingStart() DateTime {
	return DateTime{uintptr(unsafe.Pointer(C.Cronet_Metrics_sending_start_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)))))}
}

// SendingEnd
// Time when sending HTTP request body finished. (Sending request body
// happens after sending request headers.)
//
// Will equal null if the request failed or was canceled before sending
// ended.
func (m Metrics) SendingEnd() DateTime {
	return DateTime{uintptr(unsafe.Pointer(C.Cronet_Metrics_sending_end_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)))))}
}

// PushStart
// Time when first byte of HTTP/2 server push was received.  Will equal
// null if server push is not used.
func (m Metrics) PushStart() DateTime {
	return DateTime{uintptr(unsafe.Pointer(C.Cronet_Metrics_push_start_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)))))}
}

// PushEnd
// Time when last byte of HTTP/2 server push was received.  Will equal
// null if server push is not used.
func (m Metrics) PushEnd() DateTime {
	return DateTime{uintptr(unsafe.Pointer(C.Cronet_Metrics_push_end_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)))))}
}

// ResponseStart
// Time when the end of the response headers was received.
//
// Will equal null if the request failed or was canceled before the response
// started.
func (m Metrics) ResponseStart() DateTime {
	return DateTime{uintptr(unsafe.Pointer(C.Cronet_Metrics_response_start_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)))))}
}

// ResponseEnd
// Time when the request finished.
func (m Metrics) ResponseEnd() DateTime {
	return DateTime{uintptr(unsafe.Pointer(C.Cronet_Metrics_request_end_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)))))}
}

// SocketReused
// True if the socket was reused from a previous request, false otherwise.
// In HTTP/2 or QUIC, if streams are multiplexed in a single connection, this
// will be {@code true} for all streams after the first.  When {@code true},
// DNS, connection, and SSL times will be null.
func (m Metrics) SocketReused() bool {
	return bool(C.Cronet_Metrics_socket_reused_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr))))
}

// SentByteCount
// Returns total bytes sent over the network transport layer, or -1 if not
// collected.
func (m Metrics) SentByteCount() int64 {
	return int64(C.Cronet_Metrics_sent_byte_count_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr))))
}

// ReceivedByteCount
// Total bytes received over the network transport layer, or -1 if not
// collected. Number of bytes does not include any previous redirects.
func (m Metrics) ReceivedByteCount() int64 {
	return int64(C.Cronet_Metrics_received_byte_count_get(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr))))
}

func (m Metrics) SetRequestStart(t DateTime) {
	C.Cronet_Metrics_request_start_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)))
}

func (m Metrics) SetDNSStart(t DateTime) {
	C.Cronet_Metrics_dns_start_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)))
}

func (m Metrics) SetDNSEnd(t DateTime) {
	C.Cronet_Metrics_dns_end_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)))
}

func (m Metrics) SetConnectStart(t DateTime) {
	C.Cronet_Metrics_connect_start_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)))
}

func (m Metrics) SetConnectEnd(t DateTime) {
	C.Cronet_Metrics_connect_end_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)))
}

func (m Metrics) SetSSLStart(t DateTime) {
	C.Cronet_Metrics_ssl_start_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)))
}

func (m Metrics) SetSSLEnd(t DateTime) {
	C.Cronet_Metrics_ssl_end_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)))
}

func (m Metrics) SetSendingStart(t DateTime) {
	C.Cronet_Metrics_sending_start_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)))
}

func (m Metrics) SetSendingEnd(t DateTime) {
	C.Cronet_Metrics_sending_end_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)))
}

func (m Metrics) SetPushStart(t DateTime) {
	C.Cronet_Metrics_push_start_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)))
}

func (m Metrics) SetPushEnd(t DateTime) {
	C.Cronet_Metrics_push_end_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)))
}

func (m Metrics) SetResponseStart(t DateTime) {
	C.Cronet_Metrics_response_start_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)))
}

func (m Metrics) SetRequestEnd(t DateTime) {
	C.Cronet_Metrics_request_end_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)))
}

func (m Metrics) SetSocketReused(reused bool) {
	C.Cronet_Metrics_socket_reused_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.bool(reused))
}

func (m Metrics) SetSentByteCount(count int64) {
	C.Cronet_Metrics_sent_byte_count_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.int64_t(count))
}

func (m Metrics) SetReceivedByteCount(count int64) {
	C.Cronet_Metrics_received_byte_count_set(C.Cronet_MetricsPtr(unsafe.Pointer(m.ptr)), C.int64_t(count))
}
