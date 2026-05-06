package ackhandler

import (
	"github.com/sagernet/quic-go/congestion"
	cgInternal "github.com/sagernet/quic-go/internal/congestion"
	"github.com/sagernet/quic-go/internal/monotime"
	"github.com/sagernet/quic-go/internal/protocol"
)

var (
	_ cgInternal.SendAlgorithmEx             = &ccAdapterEx{}
	_ cgInternal.SendAlgorithmWithDebugInfos = &ccAdapterEx{}
)

type ccAdapterEx struct {
	CC congestion.CongestionControlEx
}

func (a *ccAdapterEx) TimeUntilSend(bytesInFlight protocol.ByteCount) monotime.Time {
	return a.CC.TimeUntilSend(congestion.ByteCount(bytesInFlight))
}

func (a *ccAdapterEx) HasPacingBudget(now monotime.Time) bool {
	return a.CC.HasPacingBudget(now)
}

func (a *ccAdapterEx) OnPacketSent(sentTime monotime.Time, bytesInFlight protocol.ByteCount, packetNumber protocol.PacketNumber, bytes protocol.ByteCount, isRetransmittable bool) {
	a.CC.OnPacketSent(sentTime, congestion.ByteCount(bytesInFlight), congestion.PacketNumber(packetNumber), congestion.ByteCount(bytes), isRetransmittable)
}

func (a *ccAdapterEx) CanSend(bytesInFlight protocol.ByteCount) bool {
	return a.CC.CanSend(congestion.ByteCount(bytesInFlight))
}

func (a *ccAdapterEx) MaybeExitSlowStart() {
	a.CC.MaybeExitSlowStart()
}

func (a *ccAdapterEx) OnPacketAcked(number protocol.PacketNumber, ackedBytes protocol.ByteCount, priorInFlight protocol.ByteCount, eventTime monotime.Time) {
	a.CC.OnPacketAcked(congestion.PacketNumber(number), congestion.ByteCount(ackedBytes), congestion.ByteCount(priorInFlight), eventTime)
}

func (a *ccAdapterEx) OnCongestionEvent(number protocol.PacketNumber, lostBytes protocol.ByteCount, priorInFlight protocol.ByteCount) {
	a.CC.OnCongestionEvent(congestion.PacketNumber(number), congestion.ByteCount(lostBytes), congestion.ByteCount(priorInFlight))
}

func (a *ccAdapterEx) OnCongestionEventEx(priorInFlight protocol.ByteCount, eventTime monotime.Time, ackedPackets []congestion.AckedPacketInfo, lostPackets []congestion.LostPacketInfo) {
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

func (a *ccAdapterEx) OnPacketsLost(leastUnacked protocol.PacketNumber) {
	a.CC.OnPacketsLost(congestion.PacketNumber(leastUnacked))
}

func (a *ccAdapterEx) OnAppLimited(bytesInFlight protocol.ByteCount) {
	a.CC.OnAppLimited(congestion.ByteCount(bytesInFlight))
}
