// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ported from:
// https://github.com/google/quiche/blob/main/quiche/quic/core/congestion_control/bbr_sender.h
// https://github.com/google/quiche/blob/main/quiche/quic/core/congestion_control/bbr_sender.cc

package congestion_bbr1

import (
	"math/rand"
	"time"

	"github.com/sagernet/quic-go/congestion"
	"github.com/sagernet/quic-go/monotime"
)

// Constants based on TCP defaults.
// See: https://github.com/google/quiche/blob/main/quiche/quic/core/quic_constants.h
const (
	// Initial congestion window in packets.
	// From quiche/quic/core/congestion_control/bbr_sender_test.cc: kInitialCongestionWindowPackets = 10
	InitialCongestionWindowPackets = 10

	// Maximum congestion window in packets.
	// From quiche/quic/core/quic_constants.h: kMaxInitialCongestionWindow = 200
	MaxCongestionWindowPackets = 200

	// Minimum congestion window in packets.
	minCongestionWindowPackets = 4

	// The minimum CWND to ensure delayed acks don't reduce bandwidth measurements.
	// Does not inflate the pacing rate.
	DefaultMinimumCongestionWindow = 4 * congestion.InitialPacketSize

	// The gain used for the STARTUP, equal to 2/ln(2).
	DefaultHighGain = 2.885
	// The newly derived gain for STARTUP, equal to 4 * ln(2).
	DerivedHighGain = 2.773
	// The newly derived CWND gain for STARTUP, 2.
	DerivedHighCWNDGain = 2.0

	// The cycle of gains used during the PROBE_BW stage.
	GainCycleLength = 8

	// The size of the bandwidth filter window, in round-trips.
	BandwidthWindowSize = GainCycleLength + 2

	// The time after which the current min_rtt value expires.
	MinRTTExpiry = 10 * time.Second
	// The minimum time the connection can spend in PROBE_RTT mode.
	ProbeRTTTime = 200 * time.Millisecond

	// If the bandwidth does not increase by the factor of StartupGrowthTarget
	// within RoundTripsWithoutGrowthBeforeExitingStartup rounds, the connection
	// will exit the STARTUP mode.
	StartupGrowthTarget                         = 1.25
	RoundTripsWithoutGrowthBeforeExitingStartup = 3

	// Default loss threshold for exiting STARTUP.
	DefaultStartupFullLossCount = 8
	DefaultBBR2LossThreshold    = 0.02
)

// PacingGain is the cycle of gains used during the PROBE_BW stage.
var PacingGain = [GainCycleLength]float64{1.25, 0.75, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}

// Mode represents the current mode of BBR.
type Mode int

const (
	// ModeStartup is the startup phase of the connection.
	ModeStartup Mode = iota
	// ModeDrain is after achieving the highest possible bandwidth during startup,
	// lower the pacing rate in order to drain the queue.
	ModeDrain
	// ModeProbeBW is the cruising mode.
	ModeProbeBW
	// ModeProbeRTT temporarily slows down sending in order to empty the buffer
	// and measure the real minimum RTT.
	ModeProbeRTT
)

// RecoveryState indicates how the congestion control limits the amount of bytes in flight.
type RecoveryState int

const (
	// RecoveryStateNotInRecovery means do not limit.
	RecoveryStateNotInRecovery RecoveryState = iota
	// RecoveryStateConservation allows an extra outstanding byte for each byte acknowledged.
	RecoveryStateConservation
	// RecoveryStateGrowth allows two extra outstanding bytes for each byte acknowledged (slow start).
	RecoveryStateGrowth
)

// BbrConfig contains configuration options for BBR.
type BbrConfig struct {
	// Number of RTTs to stay in STARTUP mode. Defaults to 3.
	NumStartupRTTs uint64
	// If true, will not exit low gain mode until bytes_in_flight drops below BDP.
	DrainToTarget bool
	// Bytes lost multiplier while detecting overshooting.
	BytesLostMultiplierWhileDetectingOvershooting uint8
	// Max ack height tracker window multiplier.
	MaxAckHeightTrackerWindowMultiplier int
	// Use derived high gain instead of default.
	UseDerivedHighGain bool
	// Enable ack aggregation during startup.
	EnableAckAggregationDuringStartup bool
	// Expire ack aggregation in startup.
	ExpireAckAggregationInStartup bool
	// Minimum congestion window in packets.
	MinCongestionWindowPackets int
	// Max congestion window with network parameters adjusted.
	MaxCongestionWindowWithNetworkParametersAdjusted congestion.ByteCount
	// Enable detect overshooting.
	DetectOvershooting bool
	// Enable overestimate avoidance (BSAO).
	OverestimateAvoidance bool
	// Start new aggregation epoch after full round (BBRA).
	StartNewAggregationEpochAfterFullRound bool
	// Limit max ack height tracker by send rate (BBRB).
	LimitMaxAckHeightTrackerBySendRate bool
	// Exit startup on loss even if app limited.
	ExitStartupOnLossEvenIfAppLimited bool
}

// DefaultBbrConfig returns the default BBR configuration.
func DefaultBbrConfig() BbrConfig {
	return BbrConfig{
		NumStartupRTTs: RoundTripsWithoutGrowthBeforeExitingStartup,
		BytesLostMultiplierWhileDetectingOvershooting:    2,
		MaxCongestionWindowWithNetworkParametersAdjusted: 200 * congestion.InitialPacketSize,
	}
}

// BbrSender implements BBR congestion control algorithm.
type BbrSender struct {
	rttStats congestion.RTTStatsProvider
	clock    Clock
	random   *rand.Rand

	mode Mode

	// Bandwidth sampler provides BBR with the bandwidth measurements at individual points.
	sampler *BandwidthSampler

	// The number of the round trips that have occurred during the connection.
	roundTripCount uint64

	// The packet number of the most recently sent packet.
	lastSentPacket congestion.PacketNumber
	// Acknowledgement of any packet after currentRoundTripEnd will cause
	// the round trip counter to advance.
	currentRoundTripEnd congestion.PacketNumber

	// Number of congestion events with some losses, in the current round.
	numLossEventsInRound int64
	// Number of total bytes lost in the current round.
	bytesLostInRound congestion.ByteCount

	// The filter that tracks the maximum bandwidth over the multiple recent round-trips.
	maxBandwidth *WindowedFilter[Bandwidth, uint64]

	// Minimum RTT estimate. Automatically expires within 10 seconds (and triggers PROBE_RTT mode).
	minRTT time.Duration
	// The time at which the current value of minRTT was assigned.
	minRTTTimestamp monotime.Time

	// The maximum allowed number of bytes in flight.
	congestionWindow congestion.ByteCount
	// The initial value of the congestionWindow.
	initialCongestionWindow congestion.ByteCount
	// The largest value the congestionWindow can achieve.
	maxCongestionWindow congestion.ByteCount
	// The smallest value the congestionWindow can achieve.
	minCongestionWindow congestion.ByteCount

	// The pacing gain applied during the STARTUP phase.
	highGain float64
	// The CWND gain applied during the STARTUP phase.
	highCWNDGain float64
	// The pacing gain applied during the DRAIN phase.
	drainGain float64

	// The current pacing rate of the connection.
	pacingRate Bandwidth
	// The gain currently applied to the pacing rate.
	pacingGain float64
	// The gain currently applied to the congestion window.
	congestionWindowGain float64
	// The gain used for the congestion window during PROBE_BW.
	congestionWindowGainConstant float64

	// The number of RTTs to stay in STARTUP mode.
	numStartupRTTs uint64

	// Number of round-trips in PROBE_BW mode, used for determining the current pacing gain cycle.
	cycleCurrentOffset int
	// The time at which the last pacing gain cycle was started.
	lastCycleStart monotime.Time

	// Indicates whether the connection has reached the full bandwidth mode.
	isAtFullBandwidth bool
	// Number of rounds during which there was no significant bandwidth increase.
	roundsWithoutBandwidthGain uint64
	// The bandwidth compared to which the increase is measured.
	bandwidthAtLastRound Bandwidth

	// Set to true upon exiting quiescence.
	exitingQuiescence bool

	// Time at which PROBE_RTT has to be exited.
	exitProbeRTTAt monotime.Time
	// Indicates whether a round-trip has passed since PROBE_RTT became active.
	probeRTTRoundPassed bool

	// Indicates whether the most recent bandwidth sample was marked as app-limited.
	lastSampleIsAppLimited bool
	// Indicates whether any non app-limited samples have been recorded.
	hasNonAppLimitedSample bool

	// Current state of recovery.
	recoveryState RecoveryState
	// Receiving acknowledgement of a packet after endRecoveryAt will cause
	// BBR to exit the recovery mode.
	endRecoveryAt congestion.PacketNumber
	// A window used to limit the number of bytes in flight during loss recovery.
	recoveryWindow congestion.ByteCount
	// If true, consider all samples in recovery app-limited.
	isAppLimitedRecovery bool

	// Configuration options
	slowerStartup                                    bool
	rateBasedStartup                                 bool
	enableAckAggregationDuringStartup                bool
	expireAckAggregationInStartup                    bool
	drainToTarget                                    bool
	detectOvershooting                               bool
	bytesLostWhileDetectingOvershooting              congestion.ByteCount
	bytesLostMultiplierWhileDetectingOvershooting    uint8
	cwndToCalculateMinPacingRate                     congestion.ByteCount
	maxCongestionWindowWithNetworkParametersAdjusted congestion.ByteCount
	exitStartupOnLossEvenIfAppLimited                bool

	// Maximum datagram size.
	maxDatagramSize congestion.ByteCount

	// Pacer for this sender.
	pacer *Pacer

	// Current bytes in flight, tracked from priorInFlight in OnCongestionEventEx.
	// This is more accurate than calculating from sampler counters because it
	// accounts for neutered packets that quic-go handles internally.
	bytesInFlight congestion.ByteCount
}

// Ensure BbrSender implements CongestionControlEx.
var _ congestion.CongestionControlEx = (*BbrSender)(nil)

// NewBbrSender creates a new BbrSender with default configuration.
func NewBbrSender(
	clock Clock,
	initialMaxDatagramSize congestion.ByteCount,
	initialCongestionWindowPackets congestion.ByteCount,
	maxCongestionWindowPackets congestion.ByteCount,
) *BbrSender {
	return NewBbrSenderWithConfig(clock, initialMaxDatagramSize, initialCongestionWindowPackets, maxCongestionWindowPackets, DefaultBbrConfig())
}

// NewBbrSenderWithConfig creates a new BbrSender with the given configuration.
func NewBbrSenderWithConfig(
	clock Clock,
	initialMaxDatagramSize congestion.ByteCount,
	initialCongestionWindowPackets congestion.ByteCount,
	maxCongestionWindowPackets congestion.ByteCount,
	config BbrConfig,
) *BbrSender {
	initialCongestionWindow := initialCongestionWindowPackets * initialMaxDatagramSize
	maxCongestionWindow := maxCongestionWindowPackets * initialMaxDatagramSize

	b := &BbrSender{
		clock:                        clock,
		random:                       rand.New(rand.NewSource(time.Now().UnixNano())),
		mode:                         ModeStartup,
		sampler:                      NewBandwidthSampler(BandwidthWindowSize),
		maxBandwidth:                 NewMaxFilter[Bandwidth, uint64](BandwidthWindowSize, 0, 0),
		congestionWindow:             initialCongestionWindow,
		initialCongestionWindow:      initialCongestionWindow,
		maxCongestionWindow:          maxCongestionWindow,
		minCongestionWindow:          minCongestionWindowForMaxDatagramSize(initialMaxDatagramSize),
		highGain:                     DefaultHighGain,
		highCWNDGain:                 DefaultHighGain,
		pacingGain:                   1,
		congestionWindowGain:         1,
		congestionWindowGainConstant: 2.0,
		numStartupRTTs:               config.NumStartupRTTs,
		recoveryState:                RecoveryStateNotInRecovery,
		recoveryWindow:               maxCongestionWindow,
		bytesLostMultiplierWhileDetectingOvershooting:    config.BytesLostMultiplierWhileDetectingOvershooting,
		cwndToCalculateMinPacingRate:                     initialCongestionWindow,
		maxCongestionWindowWithNetworkParametersAdjusted: config.MaxCongestionWindowWithNetworkParametersAdjusted,
		maxDatagramSize:                   initialMaxDatagramSize,
		drainToTarget:                     config.DrainToTarget,
		enableAckAggregationDuringStartup: config.EnableAckAggregationDuringStartup,
		expireAckAggregationInStartup:     config.ExpireAckAggregationInStartup,
		detectOvershooting:                config.DetectOvershooting,
		exitStartupOnLossEvenIfAppLimited: config.ExitStartupOnLossEvenIfAppLimited,
	}

	// Apply derived gains
	if config.UseDerivedHighGain {
		b.highGain = DerivedHighGain
		b.highCWNDGain = DerivedHighGain
		b.drainGain = 1.0 / DerivedHighCWNDGain
	} else {
		b.drainGain = 1.0 / DefaultHighGain
	}
	b.setHighCWNDGain(DerivedHighCWNDGain)

	// Apply configuration options
	if config.OverestimateAvoidance {
		b.sampler.EnableOverestimateAvoidance()
	}
	if config.StartNewAggregationEpochAfterFullRound {
		b.sampler.SetStartNewAggregationEpochAfterFullRound(true)
	}
	if config.LimitMaxAckHeightTrackerBySendRate {
		b.sampler.SetLimitMaxAckHeightTrackerBySendRate(true)
	}
	if config.MaxAckHeightTrackerWindowMultiplier > 0 {
		b.sampler.SetMaxAckHeightTrackerWindowLength(
			uint64(config.MaxAckHeightTrackerWindowMultiplier) * BandwidthWindowSize,
		)
	}
	if config.MinCongestionWindowPackets > 0 {
		b.minCongestionWindow = congestion.ByteCount(config.MinCongestionWindowPackets) * initialMaxDatagramSize
	}

	b.enterStartupMode(clock.Now())

	// Create pacer
	b.pacer = NewPacer(func() Bandwidth {
		return b.PacingRate()
	})
	b.pacer.SetMaxDatagramSize(initialMaxDatagramSize)

	return b
}

func minCongestionWindowForMaxDatagramSize(maxDatagramSize congestion.ByteCount) congestion.ByteCount {
	return minCongestionWindowPackets * maxDatagramSize
}

func scaleByteWindowForDatagramSize(window, oldMaxDatagramSize, newMaxDatagramSize congestion.ByteCount) congestion.ByteCount {
	if oldMaxDatagramSize == newMaxDatagramSize {
		return window
	}
	return congestion.ByteCount(uint64(window) * uint64(newMaxDatagramSize) / uint64(oldMaxDatagramSize))
}

func (b *BbrSender) rescalePacketSizedWindows(maxDatagramSize congestion.ByteCount) {
	oldMaxDatagramSize := b.maxDatagramSize
	b.maxDatagramSize = maxDatagramSize
	b.initialCongestionWindow = scaleByteWindowForDatagramSize(b.initialCongestionWindow, oldMaxDatagramSize, maxDatagramSize)
	b.maxCongestionWindow = scaleByteWindowForDatagramSize(b.maxCongestionWindow, oldMaxDatagramSize, maxDatagramSize)
	b.minCongestionWindow = scaleByteWindowForDatagramSize(b.minCongestionWindow, oldMaxDatagramSize, maxDatagramSize)
	b.cwndToCalculateMinPacingRate = scaleByteWindowForDatagramSize(b.cwndToCalculateMinPacingRate, oldMaxDatagramSize, maxDatagramSize)
	b.maxCongestionWindowWithNetworkParametersAdjusted = scaleByteWindowForDatagramSize(
		b.maxCongestionWindowWithNetworkParametersAdjusted,
		oldMaxDatagramSize,
		maxDatagramSize,
	)
}

// setHighCWNDGain sets the CWND gain used in STARTUP.
func (b *BbrSender) setHighCWNDGain(highCWNDGain float64) {
	b.highCWNDGain = highCWNDGain
	if b.mode == ModeStartup {
		b.congestionWindowGain = highCWNDGain
	}
}

// SetRTTStatsProvider sets the RTT stats provider.
func (b *BbrSender) SetRTTStatsProvider(provider congestion.RTTStatsProvider) {
	b.rttStats = provider
}

// TimeUntilSend returns the time until the next packet can be sent.
func (b *BbrSender) TimeUntilSend(bytesInFlight congestion.ByteCount) monotime.Time {
	return b.pacer.TimeUntilSend()
}

// HasPacingBudget returns whether the pacer has budget to send.
func (b *BbrSender) HasPacingBudget(now monotime.Time) bool {
	return b.pacer.Budget(now) >= b.maxDatagramSize
}

// OnPacketSent is called when a packet is sent.
func (b *BbrSender) OnPacketSent(
	sentTime monotime.Time,
	bytesInFlight congestion.ByteCount,
	packetNumber congestion.PacketNumber,
	bytes congestion.ByteCount,
	isRetransmittable bool,
) {
	b.pacer.OnPacketSent(sentTime, bytes)
	b.lastSentPacket = packetNumber

	// CRITICAL: Check both bytesInFlight == 0 AND sampler.IsAppLimited()
	// This is the fix for Bug 2 in bbr1_report.md
	if bytesInFlight == 0 && b.sampler.IsAppLimited() {
		b.exitingQuiescence = true
	}

	b.sampler.OnPacketSent(sentTime, packetNumber, bytes, bytesInFlight, isRetransmittable)
}

// CanSend returns whether the sender can send more data.
func (b *BbrSender) CanSend(bytesInFlight congestion.ByteCount) bool {
	return bytesInFlight < b.GetCongestionWindow()
}

// MaybeExitSlowStart is not used by BBR.
func (b *BbrSender) MaybeExitSlowStart() {}

// OnPacketAcked is not used by BBR (uses OnCongestionEventEx instead).
func (b *BbrSender) OnPacketAcked(number congestion.PacketNumber, ackedBytes congestion.ByteCount, priorInFlight congestion.ByteCount, eventTime monotime.Time) {
}

// OnCongestionEvent is not used by BBR (uses OnCongestionEventEx instead).
func (b *BbrSender) OnCongestionEvent(number congestion.PacketNumber, lostBytes congestion.ByteCount, priorInFlight congestion.ByteCount) {
}

// OnCongestionEventEx is called when packets are acked or lost.
func (b *BbrSender) OnCongestionEventEx(
	priorInFlight congestion.ByteCount,
	eventTime monotime.Time,
	ackedPackets []congestion.AckedPacketInfo,
	lostPackets []congestion.LostPacketInfo,
) {
	// Track bytesInFlight from priorInFlight provided by quic-go.
	// This is more accurate than sampler-based calculation because quic-go's
	// sent_packet_handler correctly handles neutered packets.
	b.bytesInFlight = priorInFlight
	for _, p := range ackedPackets {
		if b.bytesInFlight >= p.BytesAcked {
			b.bytesInFlight -= p.BytesAcked
		} else {
			b.bytesInFlight = 0
		}
	}
	for _, p := range lostPackets {
		if b.bytesInFlight >= p.BytesLost {
			b.bytesInFlight -= p.BytesLost
		} else {
			b.bytesInFlight = 0
		}
	}

	totalBytesAckedBefore := b.sampler.TotalBytesAcked()
	totalBytesLostBefore := b.sampler.TotalBytesLost()

	var isRoundStart bool
	var minRTTExpired bool
	var excessAcked congestion.ByteCount
	var bytesLost congestion.ByteCount

	var lastPacketSendState SendTimeState

	if len(ackedPackets) > 0 {
		lastAckedPacket := ackedPackets[len(ackedPackets)-1].PacketNumber
		isRoundStart = b.updateRoundTripCounter(lastAckedPacket)
		b.updateRecoveryState(lastAckedPacket, len(lostPackets) > 0, isRoundStart)
	}

	sample := b.sampler.OnCongestionEvent(
		eventTime,
		ackedPackets,
		lostPackets,
		b.maxBandwidth.GetBest(),
		InfiniteBandwidth(),
		b.roundTripCount,
	)

	if sample.LastPacketSendState.IsValid {
		b.lastSampleIsAppLimited = sample.LastPacketSendState.IsAppLimited
		b.hasNonAppLimitedSample = b.hasNonAppLimitedSample || !b.lastSampleIsAppLimited
	}

	// Avoid updating maxBandwidth if this is a loss-only event or all packets
	// in ackedPackets did not generate valid samples.
	if totalBytesAckedBefore != b.sampler.TotalBytesAcked() {
		if !sample.SampleIsAppLimited || sample.SampleMaxBandwidth > b.maxBandwidth.GetBest() {
			b.maxBandwidth.Update(sample.SampleMaxBandwidth, b.roundTripCount)
		}
	}

	if sample.SampleRTT > 0 && sample.SampleRTT < time.Duration(1<<63-1) {
		minRTTExpired = b.maybeUpdateMinRTT(eventTime, sample.SampleRTT)
	}

	bytesLost = b.sampler.TotalBytesLost() - totalBytesLostBefore
	excessAcked = sample.ExtraAcked
	lastPacketSendState = sample.LastPacketSendState

	if len(lostPackets) > 0 {
		b.numLossEventsInRound++
		b.bytesLostInRound += bytesLost
	}

	// Handle logic specific to PROBE_BW mode.
	if b.mode == ModeProbeBW {
		b.updateGainCyclePhase(eventTime, priorInFlight, len(lostPackets) > 0)
	}

	// Handle logic specific to STARTUP and DRAIN modes.
	if isRoundStart && !b.isAtFullBandwidth {
		b.checkIfFullBandwidthReached(&lastPacketSendState)
	}
	b.maybeExitStartupOrDrain(eventTime)

	// Handle logic specific to PROBE_RTT.
	b.maybeEnterOrExitProbeRTT(eventTime, isRoundStart, minRTTExpired)

	// Calculate number of packets acked and lost.
	bytesAcked := b.sampler.TotalBytesAcked() - totalBytesAckedBefore

	// After the model is updated, recalculate the pacing rate and congestion window.
	b.calculatePacingRate(bytesLost)
	b.calculateCongestionWindow(bytesAcked, excessAcked)
	b.calculateRecoveryWindow(bytesAcked, bytesLost)

	if isRoundStart {
		b.numLossEventsInRound = 0
		b.bytesLostInRound = 0
	}
}

// OnPacketsLost is called to notify about the least unacked packet.
func (b *BbrSender) OnPacketsLost(leastUnacked congestion.PacketNumber) {
	b.sampler.RemoveObsoletePackets(leastUnacked)
}

// OnAppLimited is called when the application has no data to send.
func (b *BbrSender) OnAppLimited(bytesInFlight congestion.ByteCount) {
	if bytesInFlight >= b.GetCongestionWindow() {
		return
	}
	b.sampler.OnAppLimited()
}

// OnRetransmissionTimeout is not used by BBR.
func (b *BbrSender) OnRetransmissionTimeout(packetsRetransmitted bool) {}

// SetMaxDatagramSize sets the maximum datagram size.
func (b *BbrSender) SetMaxDatagramSize(size congestion.ByteCount) {
	if size < b.maxDatagramSize {
		panic("cannot decrease max datagram size")
	}
	oldMinCongestionWindow := b.minCongestionWindow
	oldInitialCongestionWindow := b.initialCongestionWindow
	b.rescalePacketSizedWindows(size)
	switch b.congestionWindow {
	case oldMinCongestionWindow:
		b.congestionWindow = b.minCongestionWindow
	case oldInitialCongestionWindow:
		b.congestionWindow = b.initialCongestionWindow
	default:
		b.congestionWindow = min(b.maxCongestionWindow, max(b.congestionWindow, b.minCongestionWindow))
	}
	b.recoveryWindow = min(b.maxCongestionWindow, max(b.recoveryWindow, b.minCongestionWindow))
	b.pacer.SetMaxDatagramSize(size)
}

// InSlowStart returns whether the sender is in slow start (STARTUP mode).
func (b *BbrSender) InSlowStart() bool {
	return b.mode == ModeStartup
}

// InRecovery returns whether the sender is in recovery.
func (b *BbrSender) InRecovery() bool {
	return b.recoveryState != RecoveryStateNotInRecovery
}

// GetCongestionWindow returns the current congestion window.
func (b *BbrSender) GetCongestionWindow() congestion.ByteCount {
	if b.mode == ModeProbeRTT {
		return b.probeRTTCongestionWindow()
	}

	if b.InRecovery() {
		return min(b.congestionWindow, b.recoveryWindow)
	}

	return b.congestionWindow
}

// PacingRate returns the current pacing rate.
func (b *BbrSender) PacingRate() Bandwidth {
	if b.pacingRate.IsZero() {
		return Bandwidth(float64(BandwidthFromBytesAndTimeDelta(b.initialCongestionWindow, b.getMinRTT())) * b.highGain)
	}
	return b.pacingRate
}

// BandwidthEstimate returns the current bandwidth estimate.
func (b *BbrSender) BandwidthEstimate() Bandwidth {
	return b.maxBandwidth.GetBest()
}

// getMinRTT returns the minimum RTT estimate.
// This is equivalent to QUICHE's MinOrInitialRtt().
func (b *BbrSender) getMinRTT() time.Duration {
	if b.minRTT > 0 {
		return b.minRTT
	}
	// Fallback to RTTStats for initial estimate.
	// Use MinRTT() which returns either the minimum observed RTT or the initial RTT,
	// semantically matching QUICHE's MinOrInitialRtt(). MinRTT() is initialized to
	// DefaultInitialRTT (100ms) and is updated by SetInitialRTT() when restoring
	// from a session token.
	if b.rttStats != nil {
		minRTT := b.rttStats.MinRTT()
		if minRTT > 0 {
			return minRTT
		}
	}
	return 100 * time.Millisecond // Default initial RTT
}

// getTargetCongestionWindow computes the target congestion window using the specified gain.
func (b *BbrSender) getTargetCongestionWindow(gain float64) congestion.ByteCount {
	bdp := b.BandwidthEstimate().ToBytesPerPeriod(b.getMinRTT())
	congestionWindow := congestion.ByteCount(float64(bdp) * gain)

	// BDP estimate will be zero if no bandwidth samples are available yet.
	if congestionWindow == 0 {
		congestionWindow = congestion.ByteCount(float64(b.initialCongestionWindow) * gain)
	}

	return max(congestionWindow, b.minCongestionWindow)
}

// probeRTTCongestionWindow returns the target congestion window during PROBE_RTT.
func (b *BbrSender) probeRTTCongestionWindow() congestion.ByteCount {
	return b.minCongestionWindow
}

// enterStartupMode enters the STARTUP mode.
func (b *BbrSender) enterStartupMode(now monotime.Time) {
	b.mode = ModeStartup
	b.pacingGain = b.highGain
	b.congestionWindowGain = b.highCWNDGain
}

// enterProbeBandwidthMode enters the PROBE_BW mode.
func (b *BbrSender) enterProbeBandwidthMode(now monotime.Time) {
	b.mode = ModeProbeBW
	b.congestionWindowGain = b.congestionWindowGainConstant

	// Pick a random offset for the gain cycle out of {0, 2..7} range. 1 is
	// excluded because in that case increased gain and decreased gain would not
	// follow each other.
	b.cycleCurrentOffset = b.random.Intn(GainCycleLength - 1)
	if b.cycleCurrentOffset >= 1 {
		b.cycleCurrentOffset++
	}

	b.lastCycleStart = now
	b.pacingGain = PacingGain[b.cycleCurrentOffset]
}

// updateRoundTripCounter updates the round-trip counter if a round-trip has passed.
func (b *BbrSender) updateRoundTripCounter(lastAckedPacket congestion.PacketNumber) bool {
	if b.currentRoundTripEnd == 0 || lastAckedPacket > b.currentRoundTripEnd {
		b.roundTripCount++
		b.currentRoundTripEnd = b.lastSentPacket
		return true
	}
	return false
}

// maybeUpdateMinRTT updates the min RTT estimate if appropriate.
func (b *BbrSender) maybeUpdateMinRTT(now monotime.Time, sampleMinRTT time.Duration) bool {
	// Do not expire min_rtt if none was ever available.
	minRTTExpired := b.minRTT > 0 && now.Sub(b.minRTTTimestamp) > MinRTTExpiry

	if minRTTExpired || sampleMinRTT < b.minRTT || b.minRTT == 0 {
		b.minRTT = sampleMinRTT
		b.minRTTTimestamp = now
	}

	return minRTTExpired
}

// updateGainCyclePhase updates the current gain used in PROBE_BW mode.
func (b *BbrSender) updateGainCyclePhase(now monotime.Time, priorInFlight congestion.ByteCount, hasLosses bool) {
	// In most cases, the cycle is advanced after an RTT passes.
	shouldAdvanceGainCycling := now.Sub(b.lastCycleStart) > b.getMinRTT()

	// If the pacing gain is above 1.0, the connection is trying to probe the
	// bandwidth by increasing the number of bytes in flight to at least
	// pacing_gain * BDP. Make sure that it actually reaches the target.
	if b.pacingGain > 1.0 && !hasLosses && priorInFlight < b.getTargetCongestionWindow(b.pacingGain) {
		shouldAdvanceGainCycling = false
	}

	// If pacing gain is below 1.0, the connection is trying to drain the extra
	// queue which could have been incurred by probing prior to it.
	if b.pacingGain < 1.0 {
		// Get current bytes in flight from sampler
		bytesInFlight := b.bytesInFlight
		if bytesInFlight <= b.getTargetCongestionWindow(1) {
			shouldAdvanceGainCycling = true
		}
	}

	if shouldAdvanceGainCycling {
		b.cycleCurrentOffset = (b.cycleCurrentOffset + 1) % GainCycleLength
		b.lastCycleStart = now
		// Stay in low gain mode until the target BDP is hit.
		if b.drainToTarget && b.pacingGain < 1 &&
			PacingGain[b.cycleCurrentOffset] == 1 {
			bytesInFlight := b.bytesInFlight
			if bytesInFlight > b.getTargetCongestionWindow(1) {
				return
			}
		}
		b.pacingGain = PacingGain[b.cycleCurrentOffset]
	}
}

// checkIfFullBandwidthReached tracks for how many round-trips the bandwidth has not increased significantly.
func (b *BbrSender) checkIfFullBandwidthReached(lastPacketSendState *SendTimeState) {
	if b.exitStartupOnLossEvenIfAppLimited && b.shouldExitStartupDueToLoss(lastPacketSendState) {
		b.isAtFullBandwidth = true
	}

	if b.lastSampleIsAppLimited {
		return
	}

	target := Bandwidth(float64(b.bandwidthAtLastRound) * StartupGrowthTarget)
	if b.BandwidthEstimate() >= target {
		b.bandwidthAtLastRound = b.BandwidthEstimate()
		b.roundsWithoutBandwidthGain = 0
		if b.expireAckAggregationInStartup {
			// Expire old excess delivery measurements now that bandwidth increased.
			b.sampler.ResetMaxAckHeightTracker(0, b.roundTripCount)
		}
		return
	}

	b.roundsWithoutBandwidthGain++
	if b.roundsWithoutBandwidthGain >= b.numStartupRTTs ||
		(!b.exitStartupOnLossEvenIfAppLimited && b.shouldExitStartupDueToLoss(lastPacketSendState)) {
		b.isAtFullBandwidth = true
	}
}

// shouldExitStartupDueToLoss returns whether we should exit STARTUP due to excessive loss.
func (b *BbrSender) shouldExitStartupDueToLoss(lastPacketSendState *SendTimeState) bool {
	if b.numLossEventsInRound < int64(DefaultStartupFullLossCount) || !lastPacketSendState.IsValid {
		return false
	}

	inflightAtSend := lastPacketSendState.BytesInFlight
	if inflightAtSend > 0 && b.bytesLostInRound > 0 {
		if float64(b.bytesLostInRound) > float64(inflightAtSend)*DefaultBBR2LossThreshold {
			return true
		}
	}
	return false
}

// maybeExitStartupOrDrain transitions from STARTUP to DRAIN and from DRAIN to PROBE_BW.
func (b *BbrSender) maybeExitStartupOrDrain(now monotime.Time) {
	if b.mode == ModeStartup && b.isAtFullBandwidth {
		b.onExitStartup(now)
		b.mode = ModeDrain
		b.pacingGain = b.drainGain
		b.congestionWindowGain = b.highCWNDGain
	}
	if b.mode == ModeDrain {
		bytesInFlight := b.bytesInFlight
		if bytesInFlight <= b.getTargetCongestionWindow(1) {
			b.enterProbeBandwidthMode(now)
		}
	}
}

// onExitStartup is called right before exiting STARTUP.
func (b *BbrSender) onExitStartup(now monotime.Time) {
	// Nothing special for now
}

// maybeEnterOrExitProbeRTT decides whether to enter or exit PROBE_RTT.
func (b *BbrSender) maybeEnterOrExitProbeRTT(now monotime.Time, isRoundStart, minRTTExpired bool) {
	if minRTTExpired && !b.exitingQuiescence && b.mode != ModeProbeRTT {
		if b.InSlowStart() {
			b.onExitStartup(now)
		}
		b.mode = ModeProbeRTT
		b.pacingGain = 1
		// Do not decide on the time to exit PROBE_RTT until the bytes_in_flight
		// is at the target small value.
		b.exitProbeRTTAt = 0
	}

	if b.mode == ModeProbeRTT {
		b.sampler.OnAppLimited()

		bytesInFlight := b.bytesInFlight

		if b.exitProbeRTTAt.IsZero() {
			// If the window has reached the appropriate size, schedule exiting PROBE_RTT.
			if bytesInFlight < b.probeRTTCongestionWindow()+b.maxDatagramSize {
				b.exitProbeRTTAt = now.Add(ProbeRTTTime)
				b.probeRTTRoundPassed = false
			}
		} else {
			if isRoundStart {
				b.probeRTTRoundPassed = true
			}
			if !now.Before(b.exitProbeRTTAt) && b.probeRTTRoundPassed {
				b.minRTTTimestamp = now
				if !b.isAtFullBandwidth {
					b.enterStartupMode(now)
				} else {
					b.enterProbeBandwidthMode(now)
				}
			}
		}
	}

	b.exitingQuiescence = false
}

// updateRecoveryState determines whether BBR needs to enter, exit or advance state of the recovery.
func (b *BbrSender) updateRecoveryState(lastAckedPacket congestion.PacketNumber, hasLosses, isRoundStart bool) {
	// Disable recovery in startup, if loss-based exit is enabled.
	if !b.isAtFullBandwidth {
		return
	}

	// Exit recovery when there are no losses for a round.
	if hasLosses {
		b.endRecoveryAt = b.lastSentPacket
	}

	switch b.recoveryState {
	case RecoveryStateNotInRecovery:
		// Enter conservation on the first loss.
		if hasLosses {
			b.recoveryState = RecoveryStateConservation
			// This will cause the recoveryWindow to be set to the correct
			// value in calculateRecoveryWindow().
			b.recoveryWindow = 0
			// Since the conservation phase is meant to be lasting for a whole
			// round, extend the current round as if it were started right now.
			b.currentRoundTripEnd = b.lastSentPacket
		}

	case RecoveryStateConservation:
		if isRoundStart {
			b.recoveryState = RecoveryStateGrowth
		}
		fallthrough

	case RecoveryStateGrowth:
		// Exit recovery if appropriate.
		if !hasLosses && lastAckedPacket > b.endRecoveryAt {
			b.recoveryState = RecoveryStateNotInRecovery
		}
	}
}

// calculatePacingRate determines the appropriate pacing rate for the connection.
func (b *BbrSender) calculatePacingRate(bytesLost congestion.ByteCount) {
	if b.BandwidthEstimate().IsZero() {
		return
	}

	targetRate := Bandwidth(float64(b.BandwidthEstimate()) * b.pacingGain)
	if b.isAtFullBandwidth {
		b.pacingRate = targetRate
		return
	}

	// Pace at the rate of initial_window / RTT as soon as RTT measurements are available.
	if b.pacingRate.IsZero() && b.rttStats != nil && b.rttStats.MinRTT() > 0 {
		b.pacingRate = BandwidthFromBytesAndTimeDelta(b.initialCongestionWindow, b.rttStats.MinRTT())
		return
	}

	if b.detectOvershooting {
		b.bytesLostWhileDetectingOvershooting += bytesLost
		// Check for overshooting with network parameters adjusted when pacing rate
		// > target_rate and loss has been detected.
		if b.pacingRate > targetRate && b.bytesLostWhileDetectingOvershooting > 0 {
			if b.hasNonAppLimitedSample ||
				congestion.ByteCount(b.bytesLostMultiplierWhileDetectingOvershooting)*b.bytesLostWhileDetectingOvershooting > b.initialCongestionWindow {
				// We are fairly sure overshoot happens. Slow pacing rate.
				b.pacingRate = max(
					targetRate,
					BandwidthFromBytesAndTimeDelta(b.cwndToCalculateMinPacingRate, b.getMinRTT()),
				)
				b.bytesLostWhileDetectingOvershooting = 0
				b.detectOvershooting = false
			}
		}
	}

	// Do not decrease the pacing rate during startup.
	if targetRate > b.pacingRate {
		b.pacingRate = targetRate
	}
}

// calculateCongestionWindow determines the appropriate congestion window for the connection.
func (b *BbrSender) calculateCongestionWindow(bytesAcked, excessAcked congestion.ByteCount) {
	if b.mode == ModeProbeRTT {
		return
	}

	targetWindow := b.getTargetCongestionWindow(b.congestionWindowGain)
	if b.isAtFullBandwidth {
		// Add the max recently measured ack aggregation to CWND.
		targetWindow += b.sampler.MaxAckHeight()
	} else if b.enableAckAggregationDuringStartup {
		// Add the most recent excess acked. Because CWND never decreases in
		// STARTUP, this will automatically create a very localized max filter.
		targetWindow += excessAcked
	}

	// Instead of immediately setting the target CWND as the new one, BBR grows
	// the CWND towards targetWindow by only increasing it bytesAcked at a time.
	if b.isAtFullBandwidth {
		b.congestionWindow = min(targetWindow, b.congestionWindow+bytesAcked)
	} else if b.congestionWindow < targetWindow || b.sampler.TotalBytesAcked() < b.initialCongestionWindow {
		// If the connection is not yet out of startup phase, do not decrease the window.
		b.congestionWindow = b.congestionWindow + bytesAcked
	}

	// Enforce the limits on the congestion window.
	b.congestionWindow = max(b.congestionWindow, b.minCongestionWindow)
	b.congestionWindow = min(b.congestionWindow, b.maxCongestionWindow)
}

// calculateRecoveryWindow determines the appropriate window that constrains the in-flight during recovery.
func (b *BbrSender) calculateRecoveryWindow(bytesAcked, bytesLost congestion.ByteCount) {
	if b.recoveryState == RecoveryStateNotInRecovery {
		return
	}

	// Set up the initial recovery window.
	if b.recoveryWindow == 0 {
		bytesInFlight := b.bytesInFlight
		b.recoveryWindow = bytesInFlight + bytesAcked
		b.recoveryWindow = max(b.minCongestionWindow, b.recoveryWindow)
		return
	}

	// Remove losses from the recovery window, while accounting for a potential integer underflow.
	if b.recoveryWindow >= bytesLost {
		b.recoveryWindow = b.recoveryWindow - bytesLost
	} else {
		b.recoveryWindow = b.maxDatagramSize
	}

	// In CONSERVATION mode, just subtracting losses is sufficient. In GROWTH,
	// release additional bytesAcked to achieve a slow-start-like behavior.
	if b.recoveryState == RecoveryStateGrowth {
		b.recoveryWindow += bytesAcked
	}

	// Always allow sending at least bytesAcked in response.
	bytesInFlight := b.bytesInFlight
	b.recoveryWindow = max(b.recoveryWindow, bytesInFlight+bytesAcked)
	b.recoveryWindow = max(b.minCongestionWindow, b.recoveryWindow)
}
