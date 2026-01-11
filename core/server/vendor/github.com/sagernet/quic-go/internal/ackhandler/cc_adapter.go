package ackhandler

import (
	"time"

	"github.com/sagernet/quic-go/congestion"
	cgInternal "github.com/sagernet/quic-go/internal/congestion"
	"github.com/sagernet/quic-go/internal/protocol"
)

var _ cgInternal.SendAlgorithmWithDebugInfos = &ccAdapter{}

type ccAdapter struct {
	CC congestion.CongestionControl
}

func (a *ccAdapter) TimeUntilSend(bytesInFlight protocol.ByteCount) time.Time {
	return a.CC.TimeUntilSend(congestion.ByteCount(bytesInFlight))
}

func (a *ccAdapter) HasPacingBudget(now time.Time) bool {
	return a.CC.HasPacingBudget(now)
}

func (a *ccAdapter) OnPacketSent(sentTime time.Time, bytesInFlight protocol.ByteCount, packetNumber protocol.PacketNumber, bytes protocol.ByteCount, isRetransmittable bool) {
	a.CC.OnPacketSent(sentTime, congestion.ByteCount(bytesInFlight), congestion.PacketNumber(packetNumber), congestion.ByteCount(bytes), isRetransmittable)
}

func (a *ccAdapter) CanSend(bytesInFlight protocol.ByteCount) bool {
	return a.CC.CanSend(congestion.ByteCount(bytesInFlight))
}

func (a *ccAdapter) MaybeExitSlowStart() {
	a.CC.MaybeExitSlowStart()
}

func (a *ccAdapter) OnPacketAcked(number protocol.PacketNumber, ackedBytes protocol.ByteCount, priorInFlight protocol.ByteCount, eventTime time.Time) {
	a.CC.OnPacketAcked(congestion.PacketNumber(number), congestion.ByteCount(ackedBytes), congestion.ByteCount(priorInFlight), eventTime)
}

func (a *ccAdapter) OnCongestionEvent(number protocol.PacketNumber, lostBytes protocol.ByteCount, priorInFlight protocol.ByteCount) {
	a.CC.OnCongestionEvent(congestion.PacketNumber(number), congestion.ByteCount(lostBytes), congestion.ByteCount(priorInFlight))
}

func (a *ccAdapter) OnRetransmissionTimeout(packetsRetransmitted bool) {
	a.CC.OnRetransmissionTimeout(packetsRetransmitted)
}

func (a *ccAdapter) SetMaxDatagramSize(size protocol.ByteCount) {
	a.CC.SetMaxDatagramSize(congestion.ByteCount(size))
}

func (a *ccAdapter) InSlowStart() bool {
	return a.CC.InSlowStart()
}

func (a *ccAdapter) InRecovery() bool {
	return a.CC.InRecovery()
}

func (a *ccAdapter) GetCongestionWindow() protocol.ByteCount {
	return protocol.ByteCount(a.CC.GetCongestionWindow())
}
