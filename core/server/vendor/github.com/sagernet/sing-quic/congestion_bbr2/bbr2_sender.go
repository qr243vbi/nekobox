// BBRv2 main sender implementation.
// This file implements the CongestionControlEx interface and ties together all BBRv2 components.
// src from: https://github.com/cloudflare/quiche/blob/master/quiche/src/recovery/gcongestion/bbr2.rs

package congestion_bbr2

import (
	"math"
	"time"

	"github.com/sagernet/quic-go/congestion"
	"github.com/sagernet/quic-go/monotime"
)

const (
	// Maximum number of mode changes allowed per congestion event.
	maxModeChangesPerCongestionEvent = 4

	// Initial congestion window in packets.
	// From cloudflare/quiche apps/src/args.rs: initial_cwnd_packets: 10
	InitialCongestionWindowPackets = 10

	// Minimum congestion window in packets.
	minCongestionWindowPackets = 4

	// Maximum congestion window in packets.
	MaxCongestionWindowPackets = 10000
)

// BBR2Sender implements BBRv2 congestion control.
type BBR2Sender struct {
	// Network model containing BBRv2 state.
	model *BBRv2NetworkModel
	// Current mode of operation.
	mode Mode
	// Cycle state for PROBE_BW.
	cycle *Cycle
	// State handlers for each mode.
	startupState  *StartupState
	drainState    *DrainState
	probeBwState  *ProbeBwState
	probeRttState *ProbeRttState

	// Current congestion window.
	cwnd congestion.ByteCount
	// Maximum segment size.
	maxDatagramSize congestion.ByteCount

	// Current pacing rate.
	pacingRate Bandwidth

	// Cwnd limits.
	cwndLimits Limits

	// Initial cwnd value.
	initialCwnd congestion.ByteCount

	// Last sample was app limited.
	lastSampleIsAppLimited bool
	// Has a non-app-limited sample been received.
	hasNonAppLimitedSample bool

	// Last time we entered quiescence.
	lastQuiescenceStart *monotime.Time

	// Parameters.
	params *Params

	// Clock for time.
	clock Clock

	// RTT stats provider
	rttStats congestion.RTTStatsProvider

	// Pacer for pacing packet sends
	pacer *pacer
}

// Compile-time check that BBR2Sender implements CongestionControlEx.
var _ congestion.CongestionControlEx = (*BBR2Sender)(nil)

// NewBBR2Sender creates a new BBRv2 sender.
// If aggressive is true, uses parameters tuned for higher bandwidth acquisition.
func NewBBR2Sender(
	clock Clock,
	maxDatagramSize congestion.ByteCount,
	initialCongestionWindow congestion.ByteCount,
	aggressive bool,
) *BBR2Sender {
	var params *Params
	if aggressive {
		params = AggressiveParams()
	} else {
		params = DefaultParams()
	}
	initialRtt := 100 * time.Millisecond // Initial RTT estimate

	model := NewBBRv2NetworkModel(params, initialRtt)

	cwnd := initialCongestionWindow
	if cwnd == 0 {
		cwnd = congestion.ByteCount(InitialCongestionWindowPackets) * maxDatagramSize
	}

	maxCwnd := congestion.ByteCount(MaxCongestionWindowPackets) * maxDatagramSize

	s := &BBR2Sender{
		model:           model,
		mode:            ModeStartup,
		cycle:           NewCycle(),
		startupState:    &StartupState{},
		drainState:      &DrainState{},
		probeBwState:    &ProbeBwState{},
		probeRttState:   NewProbeRttState(),
		cwnd:            cwnd,
		maxDatagramSize: maxDatagramSize,
		pacingRate:      initialPacingRate(cwnd, initialRtt, params),
		// cwndLimits.Lo = initial_cwnd, matching quiche behavior.
		// This is different from a fixed min_cwnd (e.g., 4 packets).
		cwndLimits:  Limits{Lo: int(cwnd), Hi: int(maxCwnd)},
		initialCwnd: cwnd,
		params:      params,
		clock:       clock,
	}

	// Initialize pacer with a callback to get the current pacing rate
	s.pacer = newPacer(func() Bandwidth {
		return s.pacingRate
	}, maxDatagramSize)

	return s
}

// initialPacingRate calculates the initial pacing rate.
func initialPacingRate(cwnd congestion.ByteCount, rtt time.Duration, params *Params) Bandwidth {
	if params.InitialPacingRateBytesPerSecond != nil {
		return BandwidthFromBytesPerSecond(*params.InitialPacingRateBytesPerSecond)
	}
	return BandwidthFromBytesAndTimeDelta(cwnd, rtt).Mul(2.885)
}

// SetRTTStatsProvider sets the RTT stats provider.
func (s *BBR2Sender) SetRTTStatsProvider(provider congestion.RTTStatsProvider) {
	s.rttStats = provider
}

// TimeUntilSend returns when the next packet should be sent.
func (s *BBR2Sender) TimeUntilSend(bytesInFlight congestion.ByteCount) monotime.Time {
	if bytesInFlight >= s.GetCongestionWindow() {
		return monotime.Time(math.MaxInt64)
	}
	return s.pacer.TimeUntilSend()
}

// HasPacingBudget returns whether there's budget available for sending.
func (s *BBR2Sender) HasPacingBudget(now monotime.Time) bool {
	return s.pacer.Budget(now) >= s.maxDatagramSize
}

// OnPacketSent handles a sent packet.
func (s *BBR2Sender) OnPacketSent(
	sentTime monotime.Time,
	bytesInFlight congestion.ByteCount,
	packetNumber congestion.PacketNumber,
	bytes congestion.ByteCount,
	isRetransmittable bool,
) {
	if bytesInFlight == 0 && s.params.AvoidUnnecessaryProbeRtt {
		s.onExitQuiescence(sentTime)
	}

	s.model.OnPacketSent(
		sentTime,
		bytesInFlight,
		packetNumber,
		bytes,
		isRetransmittable,
	)

	// Update pacer with the sent packet
	s.pacer.SentPacket(sentTime, bytes)
}

// CanSend returns whether we can send more data.
func (s *BBR2Sender) CanSend(bytesInFlight congestion.ByteCount) bool {
	return bytesInFlight < s.GetCongestionWindow()
}

// MaybeExitSlowStart is not used in BBRv2.
func (s *BBR2Sender) MaybeExitSlowStart() {
	// BBRv2 doesn't use slow start in the traditional sense.
}

// OnPacketAcked is called when a packet is acknowledged.
func (s *BBR2Sender) OnPacketAcked(
	number congestion.PacketNumber,
	ackedBytes congestion.ByteCount,
	priorInFlight congestion.ByteCount,
	eventTime monotime.Time,
) {
	// This is handled by OnCongestionEventEx
}

// OnCongestionEvent is called on congestion events.
func (s *BBR2Sender) OnCongestionEvent(
	number congestion.PacketNumber,
	lostBytes congestion.ByteCount,
	priorInFlight congestion.ByteCount,
) {
	// This is handled by OnCongestionEventEx
}

// OnCongestionEventEx handles congestion events (both acks and losses).
func (s *BBR2Sender) OnCongestionEventEx(
	priorInFlight congestion.ByteCount,
	eventTime monotime.Time,
	ackedPackets []congestion.AckedPacketInfo,
	lostPackets []congestion.LostPacketInfo,
) {
	// Build congestion event
	congestionEvent := &BBRv2CongestionEvent{
		EventTime:             eventTime,
		PriorCwnd:             s.cwnd,
		PriorBytesInFlight:    priorInFlight,
		IsProbingForBandwidth: s.isProbingForBandwidth(),
	}

	// Update network model
	s.model.OnCongestionEventStart(ackedPackets, lostPackets, congestionEvent)

	// Handle mode transitions
	modeChangesAllowed := maxModeChangesPerCongestionEvent
	for modeChangesAllowed > 0 {
		modeChanged := s.handleModeOnCongestionEvent(eventTime, congestionEvent)
		if !modeChanged {
			break
		}
		modeChangesAllowed--
	}

	// Update pacing rate and cwnd
	s.updatePacingRate(congestionEvent.BytesAcked)
	s.updateCongestionWindow(congestionEvent.BytesAcked)

	// Estimate leastUnacked for cleanup (same approach as BBRv1)
	var leastUnacked congestion.PacketNumber
	if len(ackedPackets) > 0 {
		leastUnacked = ackedPackets[len(ackedPackets)-1].PacketNumber - 2
	} else if len(lostPackets) > 0 {
		leastUnacked = lostPackets[len(lostPackets)-1].PacketNumber + 1
	}

	// Finish network model processing
	s.model.OnCongestionEventFinish(leastUnacked, congestionEvent)

	s.lastSampleIsAppLimited = congestionEvent.LastPacketSendState.IsAppLimited
	if !s.lastSampleIsAppLimited {
		s.hasNonAppLimitedSample = true
	}

	if congestionEvent.BytesInFlight == 0 && s.params.AvoidUnnecessaryProbeRtt {
		s.onEnterQuiescence(eventTime)
	}
}

// handleModeOnCongestionEvent handles mode-specific congestion event processing.
// Returns true if mode changed.
func (s *BBR2Sender) handleModeOnCongestionEvent(
	eventTime monotime.Time,
	congestionEvent *BBRv2CongestionEvent,
) bool {
	targetBytesInflight := s.targetBytesInflight()
	var nextMode Mode
	var modeChanged bool

	switch s.mode {
	case ModeStartup:
		nextMode, modeChanged = s.startupState.OnCongestionEvent(
			s.model, eventTime, congestionEvent, s.params, s.cwnd,
		)
	case ModeDrain:
		nextMode, modeChanged = s.drainState.OnCongestionEvent(
			s.model, eventTime, congestionEvent, s.params, s.cwnd,
		)
	case ModeProbeBw:
		nextMode, modeChanged = s.probeBwState.OnCongestionEvent(
			s.model, s.cycle, congestionEvent.PriorBytesInFlight,
			eventTime, congestionEvent, targetBytesInflight, s.params,
		)
	case ModeProbeRtt:
		nextMode, modeChanged = s.probeRttState.OnCongestionEvent(
			s.model, s.cycle, eventTime, congestionEvent, s.params,
		)
	}

	if modeChanged {
		s.transitionToMode(nextMode, eventTime, congestionEvent)
	}

	return modeChanged
}

// transitionToMode transitions to a new mode.
func (s *BBR2Sender) transitionToMode(
	newMode Mode,
	now monotime.Time,
	congestionEvent *BBRv2CongestionEvent,
) {
	// Leave current mode
	switch s.mode {
	case ModeStartup:
		s.startupState.Leave(s.model, now, congestionEvent)
	case ModeDrain:
		s.drainState.Leave(s.model, now, congestionEvent)
	case ModeProbeBw:
		s.probeBwState.Leave(s.model, s.cycle, now, congestionEvent)
	case ModeProbeRtt:
		s.probeRttState.Leave(s.model, s.cycle, now, congestionEvent)
	}

	s.mode = newMode

	// Enter new mode
	switch newMode {
	case ModeStartup:
		s.startupState.Enter(s.model, now, congestionEvent)
	case ModeDrain:
		s.drainState.Enter(s.model, now, congestionEvent, s.params)
	case ModeProbeBw:
		s.probeBwState.Enter(s.model, s.cycle, now, congestionEvent, s.params)
	case ModeProbeRtt:
		s.probeRttState.Reset()
		s.probeRttState.Enter(s.model, s.cycle, now, congestionEvent, s.params)
	}
}

// OnRetransmissionTimeout handles retransmission timeout.
func (s *BBR2Sender) OnRetransmissionTimeout(packetsRetransmitted bool) {
	// BBRv2 doesn't do anything special on RTO.
}

// GetCongestionWindow returns the current congestion window.
func (s *BBR2Sender) GetCongestionWindow() congestion.ByteCount {
	return s.cwnd
}

// SetMaxDatagramSize updates the max datagram size.
func (s *BBR2Sender) SetMaxDatagramSize(size congestion.ByteCount) {
	if s.maxDatagramSize == size {
		return
	}

	// Scale cwnd proportionally
	factor := float64(size) / float64(s.maxDatagramSize)
	s.cwndLimits.Hi = int(float64(s.cwndLimits.Hi) * factor)
	s.cwndLimits.Lo = int(float64(s.cwndLimits.Lo) * factor)
	s.cwnd = congestion.ByteCount(float64(s.cwnd) * factor)
	s.initialCwnd = congestion.ByteCount(float64(s.initialCwnd) * factor)

	if s.params.ScalePacingRateByMss {
		s.pacingRate = s.pacingRate.Mul(factor)
	}

	s.maxDatagramSize = size

	// Update pacer's max datagram size
	s.pacer.SetMaxDatagramSize(size)
}

// InSlowStart returns whether we're in slow start (STARTUP mode).
func (s *BBR2Sender) InSlowStart() bool {
	return s.mode == ModeStartup
}

// InRecovery returns whether we're in recovery.
// BBRv2 doesn't have a traditional Recovery mode like CUBIC.
func (s *BBR2Sender) InRecovery() bool {
	return false
}

// PacingRate returns the current pacing rate.
func (s *BBR2Sender) PacingRate() Bandwidth {
	return s.pacingRate
}

// BandwidthEstimate returns the current bandwidth estimate.
func (s *BBR2Sender) BandwidthEstimate() Bandwidth {
	return s.model.BandwidthEstimate()
}

// isProbingForBandwidth returns whether we're currently probing for bandwidth.
func (s *BBR2Sender) isProbingForBandwidth() bool {
	switch s.mode {
	case ModeStartup:
		return s.startupState.IsProbingForBandwidth()
	case ModeDrain:
		return s.drainState.IsProbingForBandwidth()
	case ModeProbeBw:
		return s.probeBwState.IsProbingForBandwidth(s.cycle)
	case ModeProbeRtt:
		return s.probeRttState.IsProbingForBandwidth()
	}
	return false
}

// getCwndLimits returns the current cwnd limits based on mode.
func (s *BBR2Sender) getCwndLimits() Limits {
	switch s.mode {
	case ModeStartup:
		return s.startupState.GetCwndLimits(s.model, s.params)
	case ModeDrain:
		return s.drainState.GetCwndLimits(s.model, s.params)
	case ModeProbeBw:
		return s.probeBwState.GetCwndLimits(s.model, s.cycle, s.params)
	case ModeProbeRtt:
		return s.probeRttState.GetCwndLimits(s.model, s.cycle, s.params)
	}
	return Limits{Lo: 0, Hi: math.MaxInt}
}

// getTargetCongestionWindow calculates the target cwnd for the given gain.
func (s *BBR2Sender) getTargetCongestionWindow(gain float64) congestion.ByteCount {
	target := s.model.BDP(s.model.BandwidthEstimate(), gain)
	minTarget := congestion.ByteCount(s.cwndLimits.Lo)
	if target < minTarget {
		return minTarget
	}
	return target
}

// targetBytesInflight returns the target bytes in flight.
func (s *BBR2Sender) targetBytesInflight() congestion.ByteCount {
	bdp := s.model.BDP1(s.model.BandwidthEstimate())
	cwnd := s.GetCongestionWindow()
	if bdp < cwnd {
		return bdp
	}
	return cwnd
}

// updatePacingRate updates the pacing rate.
func (s *BBR2Sender) updatePacingRate(bytesAcked congestion.ByteCount) {
	bandwidthEstimate := s.model.BandwidthEstimate()
	if bandwidthEstimate.IsZero() {
		return
	}

	if s.model.TotalBytesAcked() == bytesAcked {
		// After the first ACK, cwnd is still the initial congestion window.
		s.pacingRate = BandwidthFromBytesAndTimeDelta(s.cwnd, s.model.MinRtt())

		if s.params.InitialPacingRateBytesPerSecond != nil {
			// Do not allow the pacing rate calculated from the first RTT
			// measurement to be higher than the configured initial pacing rate.
			initialRate := BandwidthFromBytesPerSecond(*s.params.InitialPacingRateBytesPerSecond)
			if s.pacingRate > initialRate {
				s.pacingRate = initialRate
			}
		}
		return
	}

	targetRate := bandwidthEstimate.Mul(s.model.PacingGain())
	if s.model.FullBandwidthReached() {
		s.pacingRate = targetRate
		return
	}

	if s.params.DecreaseStartupPacingAtEndOfRound &&
		s.model.PacingGain() < s.params.StartupPacingGain {
		s.pacingRate = targetRate
		return
	}

	if s.params.BwLoMode != BwLoModeDefault &&
		s.model.LossEventsInRound() > 0 {
		s.pacingRate = targetRate
		return
	}

	// By default, the pacing rate never decreases in STARTUP.
	if targetRate > s.pacingRate {
		s.pacingRate = targetRate
	}
}

// updateCongestionWindow updates the congestion window.
func (s *BBR2Sender) updateCongestionWindow(bytesAcked congestion.ByteCount) {
	targetCwnd := s.getTargetCongestionWindow(s.model.CwndGain())

	priorCwnd := s.cwnd
	if s.model.FullBandwidthReached() {
		targetCwnd += s.model.MaxAckHeight()
		newCwnd := priorCwnd + bytesAcked
		if targetCwnd < newCwnd {
			s.cwnd = targetCwnd
		} else {
			s.cwnd = newCwnd
		}
	} else if priorCwnd < targetCwnd || priorCwnd < 2*s.initialCwnd {
		s.cwnd = priorCwnd + bytesAcked
	}

	// Apply mode limits
	modeLimits := s.getCwndLimits()
	s.cwnd = congestion.ByteCount(modeLimits.ApplyLimits(int(s.cwnd)))

	// Apply global limits
	s.cwnd = congestion.ByteCount(s.cwndLimits.ApplyLimits(int(s.cwnd)))
}

// onExitQuiescence handles exiting quiescence.
func (s *BBR2Sender) onExitQuiescence(now monotime.Time) {
	if s.lastQuiescenceStart == nil {
		return
	}

	quiescenceStart := *s.lastQuiescenceStart
	s.lastQuiescenceStart = nil

	var nextMode Mode

	switch s.mode {
	case ModeStartup:
		nextMode = s.startupState.OnExitQuiescence(s.model, now, quiescenceStart, s.params)
	case ModeDrain:
		nextMode = s.drainState.OnExitQuiescence(s.model, now, quiescenceStart, s.params)
	case ModeProbeBw:
		nextMode = s.probeBwState.OnExitQuiescence(s.model, s.cycle, now, quiescenceStart, s.params)
	case ModeProbeRtt:
		nextMode = s.probeRttState.OnExitQuiescence(s.model, s.cycle, now, quiescenceStart, s.params)
	}

	if nextMode != s.mode {
		s.transitionToMode(nextMode, now, nil)
	}
}

// onEnterQuiescence handles entering quiescence.
func (s *BBR2Sender) onEnterQuiescence(now monotime.Time) {
	s.lastQuiescenceStart = &now
}

// OnPacketNeutered handles a neutered packet.
func (s *BBR2Sender) OnPacketNeutered(packetNumber congestion.PacketNumber) {
	s.model.OnPacketNeutered(packetNumber)
}

// OnAppLimited marks the connection as app limited.
func (s *BBR2Sender) OnAppLimited(bytesInFlight congestion.ByteCount) {
	if bytesInFlight >= s.GetCongestionWindow() {
		return
	}
	s.model.OnAppLimited()
}

// OnPacketsLost is called to notify the congestion controller about the lowest unacked packet number.
// This allows cleanup of obsolete packet state data to prevent memory leaks.
func (s *BBR2Sender) OnPacketsLost(leastUnacked congestion.PacketNumber) {
	s.model.CleanupObsoletePackets(leastUnacked)
}

// Mode returns the current BBRv2 mode.
func (s *BBR2Sender) Mode() Mode {
	return s.mode
}

// CyclePhase returns the current cycle phase (for PROBE_BW).
func (s *BBR2Sender) CyclePhase() CyclePhase {
	return s.cycle.Phase
}

// MinRtt returns the minimum RTT.
func (s *BBR2Sender) MinRtt() time.Duration {
	return s.model.MinRtt()
}
