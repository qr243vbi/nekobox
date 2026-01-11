package ackhandler

import (
	"time"

	"github.com/sagernet/quic-go/congestion"
	cgInternal "github.com/sagernet/quic-go/internal/congestion"
	"github.com/sagernet/quic-go/internal/protocol"
)

var (
	_ cgInternal.SendAlgorithmEx             = &ccAdapterEx{}
	_ cgInternal.SendAlgorithmWithDebugInfos = &ccAdapterEx{}
)

type ccAdapterEx struct {
	CC congestion.CongestionControlEx
}

func (a *ccAdapterEx) TimeUntilSend(bytesInFlight protocol.ByteCount) time.Time {
	return a.CC.TimeUntilSend(congestion.ByteCount(bytesInFlight))
}

func (a *ccAdapterEx) HasPacingBudget(now time.Time) bool {
	return a.CC.HasPacingBudget(now)
}

func (a *ccAdapterEx) OnPacketSent(sentTime time.Time, bytesInFlight protocol.ByteCount, packetNumber protocol.PacketNumber, bytes protocol.ByteCount, isRetransmittable bool) {
	a.CC.OnPacketSent(sentTime, congestion.ByteCount(bytesInFlight), congestion.PacketNumber(packetNumber), congestion.ByteCount(bytes), isRetransmittable)
}

func (a *ccAdapterEx) CanSend(bytesInFlight protocol.ByteCount) bool {
	return a.CC.CanSend(congestion.ByteCount(bytesInFlight))
}

func (a *ccAdapterEx) MaybeExitSlowStart() {
	a.CC.MaybeExitSlowStart()
}

func (a *ccAdapterEx) OnPacketAcked(number protocol.PacketNumber, ackedBytes protocol.ByteCount, priorInFlight protocol.ByteCount, eventTime time.Time) {
	a.CC.OnPacketAcked(congestion.PacketNumber(number), congestion.ByteCount(ackedBytes), congestion.ByteCount(priorInFlight), eventTime)
}

func (a *ccAdapterEx) OnCongestionEvent(number protocol.PacketNumber, lostBytes protocol.ByteCount, priorInFlight protocol.ByteCount) {
	a.CC.OnCongestionEvent(congestion.PacketNumber(number), congestion.ByteCount(lostBytes), congestion.ByteCount(priorInFlight))
}

func (a *ccAdapterEx) OnCongestionEventEx(priorInFlight protocol.ByteCount, eventTime time.Time, ackedPackets []congestion.AckedPacketInfo, lostPackets []congestion.LostPacketInfo) {
	a.CC.OnCongestionEventEx(congestion.ByteCount(priorInFlight), eventTime, ackedPackets, lostPackets)
}

func (a *ccAdapterEx) OnRetransmissionTimeout(packetsRetransmitted bool) {
	a.CC.OnRetransmissionTimeout(packetsRetransmitted)
}

func (a *ccAdapterEx) SetMaxDatagramSize(size protocol.ByteCount) {
	a.CC.SetMaxDatagramSize(congestion.ByteCount(size))
}

func (a *ccAdapterEx) InSlowStart() bool {
	return a.CC.InSlowStart()
}

func (a *ccAdapterEx) InRecovery() bool {
	return a.CC.InRecovery()
}

func (a *ccAdapterEx) GetCongestionWindow() protocol.ByteCount {
	return protocol.ByteCount(a.CC.GetCongestionWindow())
}
