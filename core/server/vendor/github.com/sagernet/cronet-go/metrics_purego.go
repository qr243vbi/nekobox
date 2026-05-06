//go:build with_purego

package cronet

import (
	"github.com/sagernet/cronet-go/internal/cronet"
)

func NewMetrics() Metrics {
	return Metrics{cronet.MetricsCreate()}
}

func (m Metrics) Destroy() {
	cronet.MetricsDestroy(m.ptr)
}

func (m Metrics) RequestStart() DateTime {
	return DateTime{cronet.MetricsRequestStartGet(m.ptr)}
}

func (m Metrics) DNSStart() DateTime {
	return DateTime{cronet.MetricsDnsStartGet(m.ptr)}
}

func (m Metrics) DNSEnd() DateTime {
	return DateTime{cronet.MetricsDnsEndGet(m.ptr)}
}

func (m Metrics) ConnectStart() DateTime {
	return DateTime{cronet.MetricsConnectStartGet(m.ptr)}
}

func (m Metrics) ConnectEnd() DateTime {
	return DateTime{cronet.MetricsConnectEndGet(m.ptr)}
}

func (m Metrics) SSLStart() DateTime {
	return DateTime{cronet.MetricsSslStartGet(m.ptr)}
}

func (m Metrics) SSLEnd() DateTime {
	return DateTime{cronet.MetricsSslEndGet(m.ptr)}
}

func (m Metrics) SendingStart() DateTime {
	return DateTime{cronet.MetricsSendingStartGet(m.ptr)}
}

func (m Metrics) SendingEnd() DateTime {
	return DateTime{cronet.MetricsSendingEndGet(m.ptr)}
}

func (m Metrics) PushStart() DateTime {
	return DateTime{cronet.MetricsPushStartGet(m.ptr)}
}

func (m Metrics) PushEnd() DateTime {
	return DateTime{cronet.MetricsPushEndGet(m.ptr)}
}

func (m Metrics) ResponseStart() DateTime {
	return DateTime{cronet.MetricsResponseStartGet(m.ptr)}
}

func (m Metrics) ResponseEnd() DateTime {
	return DateTime{cronet.MetricsRequestEndGet(m.ptr)}
}

func (m Metrics) SocketReused() bool {
	return cronet.MetricsSocketReusedGet(m.ptr)
}

func (m Metrics) SentByteCount() int64 {
	return cronet.MetricsSentByteCountGet(m.ptr)
}

func (m Metrics) ReceivedByteCount() int64 {
	return cronet.MetricsReceivedByteCountGet(m.ptr)
}

func (m Metrics) SetRequestStart(t DateTime) {
	cronet.MetricsRequestStartSet(m.ptr, t.ptr)
}

func (m Metrics) SetDNSStart(t DateTime) {
	cronet.MetricsDnsStartSet(m.ptr, t.ptr)
}

func (m Metrics) SetDNSEnd(t DateTime) {
	cronet.MetricsDnsEndSet(m.ptr, t.ptr)
}

func (m Metrics) SetConnectStart(t DateTime) {
	cronet.MetricsConnectStartSet(m.ptr, t.ptr)
}

func (m Metrics) SetConnectEnd(t DateTime) {
	cronet.MetricsConnectEndSet(m.ptr, t.ptr)
}

func (m Metrics) SetSSLStart(t DateTime) {
	cronet.MetricsSslStartSet(m.ptr, t.ptr)
}

func (m Metrics) SetSSLEnd(t DateTime) {
	cronet.MetricsSslEndSet(m.ptr, t.ptr)
}

func (m Metrics) SetSendingStart(t DateTime) {
	cronet.MetricsSendingStartSet(m.ptr, t.ptr)
}

func (m Metrics) SetSendingEnd(t DateTime) {
	cronet.MetricsSendingEndSet(m.ptr, t.ptr)
}

func (m Metrics) SetPushStart(t DateTime) {
	cronet.MetricsPushStartSet(m.ptr, t.ptr)
}

func (m Metrics) SetPushEnd(t DateTime) {
	cronet.MetricsPushEndSet(m.ptr, t.ptr)
}

func (m Metrics) SetResponseStart(t DateTime) {
	cronet.MetricsResponseStartSet(m.ptr, t.ptr)
}

func (m Metrics) SetRequestEnd(t DateTime) {
	cronet.MetricsRequestEndSet(m.ptr, t.ptr)
}

func (m Metrics) SetSocketReused(reused bool) {
	cronet.MetricsSocketReusedSet(m.ptr, reused)
}

func (m Metrics) SetSentByteCount(count int64) {
	cronet.MetricsSentByteCountSet(m.ptr, count)
}

func (m Metrics) SetReceivedByteCount(count int64) {
	cronet.MetricsReceivedByteCountSet(m.ptr, count)
}
