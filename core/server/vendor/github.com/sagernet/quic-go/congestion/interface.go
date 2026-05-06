package congestion

import (
	"time"

	"github.com/sagernet/quic-go/internal/protocol"
	"github.com/sagernet/quic-go/monotime"
)

type (
	ByteCount    protocol.ByteCount
	PacketNumber protocol.PacketNumber
)

// Expose some constants from protocol that congestion control algorithms may need.
const (
	InitialPacketSize          = protocol.InitialPacketSize
	MinPacingDelay             = protocol.MinPacingDelay
	MaxPacketBufferSize        = protocol.MaxPacketBufferSize
	MinInitialPacketSize       = protocol.MinInitialPacketSize
	MaxCongestionWindowPackets = protocol.MaxCongestionWindowPackets
	PacketsPerConnectionID     = protocol.PacketsPerConnectionID
)

type AckedPacketInfo struct {
	PacketNumber PacketNumber
	BytesAcked   ByteCount
	ReceivedTime monotime.Time
	SentTime     monotime.Time
}

type LostPacketInfo struct {
	PacketNumber PacketNumber
	BytesLost    ByteCount
}

type CongestionControl interface {
	SetRTTStatsProvider(provider RTTStatsProvider)
	TimeUntilSend(bytesInFlight ByteCount) monotime.Time
	HasPacingBudget(now monotime.Time) bool
	OnPacketSent(sentTime monotime.Time, bytesInFlight ByteCount, packetNumber PacketNumber, bytes ByteCount, isRetransmittable bool)
	CanSend(bytesInFlight ByteCount) bool
	MaybeExitSlowStart()
	OnPacketAcked(number PacketNumber, ackedBytes ByteCount, priorInFlight ByteCount, eventTime monotime.Time)
	OnCongestionEvent(number PacketNumber, lostBytes ByteCount, priorInFlight ByteCount)
	OnRetransmissionTimeout(packetsRetransmitted bool)
	SetMaxDatagramSize(size ByteCount)
	InSlowStart() bool
	InRecovery() bool
	GetCongestionWindow() ByteCount
}

type CongestionControlEx interface {
	CongestionControl
	OnCongestionEventEx(priorInFlight ByteCount, eventTime monotime.Time, ackedPackets []AckedPacketInfo, lostPackets []LostPacketInfo)
	// OnPacketsLost is called to notify the congestion controller about the lowest unacked packet number.
	// This allows cleanup of obsolete packet state data.
	OnPacketsLost(leastUnacked PacketNumber)
	// OnAppLimited is called when the application has no data to send but cwnd is not fully utilized.
	OnAppLimited(bytesInFlight ByteCount)
}

type RTTStatsProvider interface {
	MinRTT() time.Duration
	LatestRTT() time.Duration
	SmoothedRTT() time.Duration
	MeanDeviation() time.Duration
	MaxAckDelay() time.Duration
	PTO(includeMaxAckDelay bool) time.Duration
	UpdateRTT(sendDelta, ackDelay time.Duration)
	SetMaxAckDelay(mad time.Duration)
	SetInitialRTT(t time.Duration)
}
