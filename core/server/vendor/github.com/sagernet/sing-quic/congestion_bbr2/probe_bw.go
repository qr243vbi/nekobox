// BBRv2 PROBE_BW state implementation.
// PROBE_BW is the main steady-state where BBR cycles through probing phases
// to periodically check if more bandwidth is available.
// src from: https://github.com/cloudflare/quiche/blob/master/quiche/src/recovery/gcongestion/bbr2/probe_bw.rs

package congestion_bbr2

import (
	"time"

	"github.com/sagernet/quic-go/congestion"
	"github.com/sagernet/quic-go/monotime"
)

// ProbeBwState handles the PROBE_BW mode logic with 4 sub-states.
type ProbeBwState struct{}

// IsProbingForBandwidth returns true if in REFILL or UP phase.
func (p *ProbeBwState) IsProbingForBandwidth(cycle *Cycle) bool {
	return cycle.Phase == CyclePhaseRefill || cycle.Phase == CyclePhaseUp
}

// Enter is called when entering PROBE_BW state.
func (p *ProbeBwState) Enter(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	now monotime.Time,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
) {
	cycle.StartTime = now

	switch cycle.Phase {
	case CyclePhaseNotStarted:
		// First time entering PROBE_BW. Start a new probing cycle.
		p.enterProbeDown(model, cycle, false, false, now, params)
	case CyclePhaseCruise:
		p.enterProbeCruise(model, cycle, now)
	case CyclePhaseRefill:
		p.enterProbeRefill(model, cycle, cycle.ProbeUpRounds, now)
	case CyclePhaseUp, CyclePhaseDown:
		// Already in a valid phase, do nothing
	}
}

// Leave is called when leaving PROBE_BW state.
func (p *ProbeBwState) Leave(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	now monotime.Time,
	congestionEvent *BBRv2CongestionEvent,
) {
	// No special leave logic for PROBE_BW.
}

// GetCwndLimits returns the cwnd limits for PROBE_BW.
func (p *ProbeBwState) GetCwndLimits(model *BBRv2NetworkModel, cycle *Cycle, params *Params) Limits {
	if cycle.Phase == CyclePhaseCruise {
		limit := model.InflightLo()
		hiWithHeadroom := model.InflightHiWithHeadroom()
		if hiWithHeadroom < limit {
			limit = hiWithHeadroom
		}
		return NoGreaterThan(int(limit))
	}

	if cycle.Phase == CyclePhaseUp && params.ProbeUpIgnoreInflightHi {
		// Similar to STARTUP.
		return NoGreaterThan(int(model.InflightLo()))
	}

	limit := model.InflightLo()
	if model.InflightHi() < limit {
		limit = model.InflightHi()
	}
	return NoGreaterThan(int(limit))
}

// OnCongestionEvent handles a congestion event in PROBE_BW state.
// Returns the next mode and whether mode changed.
func (p *ProbeBwState) OnCongestionEvent(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	priorInFlight congestion.ByteCount,
	eventTime monotime.Time,
	congestionEvent *BBRv2CongestionEvent,
	targetBytesInflight congestion.ByteCount,
	params *Params,
) (nextMode Mode, modeChanged bool) {
	if congestionEvent.EndOfRoundTrip {
		if cycle.StartTime != eventTime {
			cycle.RoundsSinceProbe++
		}

		if cycle.PhaseStartTime != eventTime {
			cycle.RoundsInPhase++
		}
	}

	switchToProbeRtt := false

	switch cycle.Phase {
	case CyclePhaseNotStarted:
		// Should not happen, but handle gracefully
		p.enterProbeDown(model, cycle, false, false, eventTime, params)
	case CyclePhaseUp:
		p.updateProbeUp(model, cycle, priorInFlight, targetBytesInflight, congestionEvent, params)
	case CyclePhaseDown:
		p.updateProbeDown(model, cycle, targetBytesInflight, congestionEvent, params)
		if cycle.Phase != CyclePhaseDown && model.MaybeExpireMinRtt(congestionEvent) {
			switchToProbeRtt = true
		}
	case CyclePhaseCruise:
		p.updateProbeCruise(model, cycle, targetBytesInflight, congestionEvent, params)
	case CyclePhaseRefill:
		p.updateProbeRefill(model, cycle, targetBytesInflight, congestionEvent, params)
	}

	// Do not need to set the gains if switching to PROBE_RTT
	if !switchToProbeRtt {
		model.SetPacingGain(p.phasePacingGain(cycle.Phase, params))
		model.SetCwndGain(p.phaseCwndGain(cycle.Phase, params))
	}

	if switchToProbeRtt {
		return ModeProbeRtt, true
	}

	return ModeProbeBw, false
}

// phasePacingGain returns the pacing gain for a given phase.
func (p *ProbeBwState) phasePacingGain(phase CyclePhase, params *Params) float64 {
	switch phase {
	case CyclePhaseUp:
		return params.ProbeBwProbeUpPacingGain
	case CyclePhaseDown:
		return params.ProbeBwProbeDownPacingGain
	default:
		return params.ProbeBwDefaultPacingGain
	}
}

// phaseCwndGain returns the cwnd gain for a given phase.
func (p *ProbeBwState) phaseCwndGain(phase CyclePhase, params *Params) float64 {
	switch phase {
	case CyclePhaseUp:
		return params.ProbeBwUpCwndGain
	default:
		return params.ProbeBwCwndGain
	}
}

func (p *ProbeBwState) enterProbeDown(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	probedTooHigh bool,
	stoppedRiskyProbe bool,
	now monotime.Time,
	params *Params,
) {
	cycle.LastCycleProbedTooHigh = probedTooHigh
	cycle.LastCycleStoppedRiskyProbe = stoppedRiskyProbe

	cycle.Phase = CyclePhaseDown
	cycle.StartTime = now
	cycle.PhaseStartTime = now
	cycle.RoundsInPhase = 0

	if params.BwLoMode != BwLoModeDefault {
		// Clear bandwidth lo if it was set in PROBE_UP, because losses in
		// PROBE_UP should not permanently change bandwidth_lo.
		model.ClearBandwidthLo()
	}

	// Pick probe wait time.
	// TODO(quiche): actually pick time with randomness per RFC (BBRPickProbeWait).
	cycle.RoundsSinceProbe = 0
	cycle.ProbeWaitTime = params.ProbeBwProbeBaseDuration + 500*time.Microsecond

	cycle.ProbeUpBytes = nil
	// Note: ProbeUpAcked is NOT reset here, matching quiche behavior.
	// It will be reset in enterProbeRefill before the next PROBE_UP.
	cycle.ProbeUpAppLimitedSinceInflightHiLimited = false
	cycle.hasAdvancedMaxBw = false
	// Note: IsSampleFromProbing is NOT reset here, matching quiche behavior.
	// It remains true from PROBE_UP and is reset in updateProbeDown after
	// the first round (RoundsInPhase == 1 && EndOfRoundTrip).
	model.RestartRoundEarly()
}

func (p *ProbeBwState) enterProbeCruise(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	now monotime.Time,
) {
	if cycle.Phase == CyclePhaseDown {
		p.exitProbeDown(model, cycle)
	}

	model.CapInflightLo(model.InflightHi())
	cycle.Phase = CyclePhaseCruise
	cycle.PhaseStartTime = now
	cycle.RoundsInPhase = 0
	cycle.IsSampleFromProbing = false
}

func (p *ProbeBwState) enterProbeRefill(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	probeUpRounds int,
	now monotime.Time,
) {
	if cycle.Phase == CyclePhaseDown {
		p.exitProbeDown(model, cycle)
	}

	cycle.Phase = CyclePhaseRefill
	cycle.PhaseStartTime = now
	cycle.RoundsInPhase = 0

	cycle.IsSampleFromProbing = false
	cycle.LastCycleStoppedRiskyProbe = false

	model.ClearBandwidthLo()
	model.ClearInflightLo()
	cycle.ProbeUpRounds = probeUpRounds
	cycle.ProbeUpAcked = 0
	model.RestartRoundEarly()
}

func (p *ProbeBwState) enterProbeUp(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	now monotime.Time,
	cwnd congestion.ByteCount,
) {
	cycle.Phase = CyclePhaseUp
	cycle.PhaseStartTime = now
	cycle.RoundsInPhase = 0
	cycle.IsSampleFromProbing = true
	p.raiseInflightHighSlope(cycle, cwnd)
	model.RestartRoundEarly()
}

func (p *ProbeBwState) exitProbeDown(model *BBRv2NetworkModel, cycle *Cycle) {
	if !cycle.hasAdvancedMaxBw {
		model.AdvanceMaxBandwidthFilter()
		cycle.hasAdvancedMaxBw = true
	}
}

func (p *ProbeBwState) updateProbeDown(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	targetBytesInflight congestion.ByteCount,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
) {
	if cycle.RoundsInPhase == 1 && congestionEvent.EndOfRoundTrip {
		cycle.IsSampleFromProbing = false

		if !congestionEvent.LastPacketSendState.IsAppLimited {
			model.AdvanceMaxBandwidthFilter()
			cycle.hasAdvancedMaxBw = true
		}

		if cycle.LastCycleStoppedRiskyProbe && !cycle.LastCycleProbedTooHigh {
			p.enterProbeRefill(model, cycle, 0, congestionEvent.EventTime)
			return
		}
	}

	p.maybeAdaptUpperBounds(model, cycle, targetBytesInflight, congestionEvent, params)

	if p.isTimeToProbeBandwidth(model, cycle, targetBytesInflight, congestionEvent, params) {
		p.enterProbeRefill(model, cycle, 0, congestionEvent.EventTime)
		return
	}

	// This exit condition is experimental code from Google quiche which
	// diverges from the RFC. Use `disable_probe_down_early_exit` to override.
	if p.hasStayedLongEnoughInProbeDown(model, cycle, congestionEvent, params) {
		p.enterProbeCruise(model, cycle, congestionEvent.EventTime)
		return
	}

	inflightWithHeadroom := model.InflightHiWithHeadroom()
	bytesInFlight := congestionEvent.BytesInFlight

	if bytesInFlight > inflightWithHeadroom {
		// Stay in PROBE_DOWN.
		return
	}

	// Transition to PROBE_CRUISE iff we've drained to target.
	bdp := model.BDP0()

	if bytesInFlight < bdp {
		p.enterProbeCruise(model, cycle, congestionEvent.EventTime)
	}
}

func (p *ProbeBwState) updateProbeCruise(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	targetBytesInflight congestion.ByteCount,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
) {
	p.maybeAdaptUpperBounds(model, cycle, targetBytesInflight, congestionEvent, params)

	if p.isTimeToProbeBandwidth(model, cycle, targetBytesInflight, congestionEvent, params) {
		p.enterProbeRefill(model, cycle, 0, congestionEvent.EventTime)
	}
}

func (p *ProbeBwState) updateProbeRefill(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	targetBytesInflight congestion.ByteCount,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
) {
	p.maybeAdaptUpperBounds(model, cycle, targetBytesInflight, congestionEvent, params)

	if cycle.RoundsInPhase > 0 && congestionEvent.EndOfRoundTrip {
		p.enterProbeUp(model, cycle, congestionEvent.EventTime, congestionEvent.PriorCwnd)
	}
}

func (p *ProbeBwState) updateProbeUp(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	priorInFlight congestion.ByteCount,
	targetBytesInflight congestion.ByteCount,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
) {
	result := p.maybeAdaptUpperBounds(model, cycle, targetBytesInflight, congestionEvent, params)
	if result == AdaptUpperBoundsAdaptedProbedTooHigh {
		p.enterProbeDown(model, cycle, true, false, congestionEvent.EventTime, params)
		return
	}

	p.probeInflightHighUpward(model, cycle, congestionEvent, params)

	isRisky := false
	isQueuing := false

	if cycle.LastCycleProbedTooHigh && priorInFlight >= model.InflightHi() {
		isRisky = true
	} else if cycle.RoundsInPhase > 0 {
		if params.MaxProbeUpQueueRounds > 0 {
			if congestionEvent.EndOfRoundTrip {
				model.CheckPersistentQueue(params.FullBwThreshold)
				if model.RoundsWithQueueing() >= params.MaxProbeUpQueueRounds {
					isQueuing = true
				}
			}
		} else {
			queueingThresholdExtraBytes := model.QueueingThresholdExtraBytes()
			if params.AddAckHeightToQueueingThreshold {
				queueingThresholdExtraBytes += model.MaxAckHeight()
			}
			queueingThreshold := congestion.ByteCount(
				params.FullBwThreshold*float64(model.BDP0()),
			) + queueingThresholdExtraBytes

			isQueuing = congestionEvent.BytesInFlight >= queueingThreshold
		}
	}

	if isRisky || isQueuing {
		p.enterProbeDown(model, cycle, false, isRisky, congestionEvent.EventTime, params)
	}
}

func (p *ProbeBwState) isTimeToProbeBandwidth(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	targetBytesInflight congestion.ByteCount,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
) bool {
	if p.hasCycleLasted(cycle, cycle.ProbeWaitTime, congestionEvent) {
		return true
	}

	if p.isTimeToProbeForRenoCoexistence(cycle, targetBytesInflight, 1.0, congestionEvent, params) {
		return true
	}

	return false
}

func (p *ProbeBwState) maybeAdaptUpperBounds(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	targetBytesInflight congestion.ByteCount,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
) AdaptUpperBoundsResult {
	sendState := congestionEvent.LastPacketSendState

	if !sendState.IsValid {
		return AdaptUpperBoundsNotAdaptedInvalidSample
	}

	inflightAtSend := sendState.BytesInFlight
	if params.UseBytesDeliveredForInflightHi {
		inflightAtSend = model.TotalBytesAcked() - congestionEvent.LastPacketSendState.TotalBytesAcked
	}

	if cycle.IsSampleFromProbing {
		if model.IsInflightTooHigh(congestionEvent, params.ProbeBwFullLossCount) {
			cycle.IsSampleFromProbing = false
			if !sendState.IsAppLimited || params.MaxProbeUpQueueRounds > 0 {
				inflightTarget := congestion.ByteCount(float64(targetBytesInflight) * (1.0 - params.Beta))

				newInflightHi := inflightAtSend
				if inflightTarget > newInflightHi {
					newInflightHi = inflightTarget
				}

				if params.LimitInflightHiByMaxDelivered {
					if model.MaxBytesDeliveredInRound() > newInflightHi {
						newInflightHi = model.MaxBytesDeliveredInRound()
					}
				}

				model.SetInflightHi(newInflightHi)
			}
			return AdaptUpperBoundsAdaptedProbedTooHigh
		}
		return AdaptUpperBoundsAdaptedOk
	}

	if model.InflightHi() == model.InflightHiDefault() {
		return AdaptUpperBoundsNotAdaptedInflightHighNotSet
	}

	// Raise the upper bound for inflight.
	if inflightAtSend > model.InflightHi() {
		model.SetInflightHi(inflightAtSend)
	}

	return AdaptUpperBoundsAdaptedOk
}

func (p *ProbeBwState) hasCycleLasted(
	cycle *Cycle,
	duration time.Duration,
	congestionEvent *BBRv2CongestionEvent,
) bool {
	return congestionEvent.EventTime.Sub(cycle.StartTime) > duration
}

func (p *ProbeBwState) hasPhaseLasted(
	cycle *Cycle,
	duration time.Duration,
	congestionEvent *BBRv2CongestionEvent,
) bool {
	return congestionEvent.EventTime.Sub(cycle.PhaseStartTime) > duration
}

func (p *ProbeBwState) isTimeToProbeForRenoCoexistence(
	cycle *Cycle,
	targetBytesInflight congestion.ByteCount,
	probeWaitFraction float64,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
) bool {
	if !params.EnableRenoCoexistence {
		return false
	}

	rounds := params.ProbeBwProbeMaxRounds
	if params.ProbeBwProbeRenoGain > 0.0 {
		renoRounds := int(params.ProbeBwProbeRenoGain * float64(targetBytesInflight) / float64(defaultMSS))
		if renoRounds < rounds {
			rounds = renoRounds
		}
	}

	return cycle.RoundsSinceProbe >= int(float64(rounds)*probeWaitFraction)
}

// hasStayedLongEnoughInProbeDown checks if we've stayed long enough in PROBE_DOWN.
// This is experimental code from Google quiche and diverges from the RFC.
func (p *ProbeBwState) hasStayedLongEnoughInProbeDown(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
) bool {
	if params.DisableProbeDownEarlyExit {
		return false
	}

	// Stay in PROBE_DOWN for at most the time of a min rtt, as it is done in BBRv1.
	return p.hasPhaseLasted(cycle, model.MinRtt(), congestionEvent)
}

func (p *ProbeBwState) raiseInflightHighSlope(cycle *Cycle, cwnd congestion.ByteCount) {
	growthThisRound := 1 << cycle.ProbeUpRounds
	// Cap probe_up_rounds at 30 so growth doesn't exceed 1G
	if cycle.ProbeUpRounds < 30 {
		cycle.ProbeUpRounds++
	}
	probeUpBytes := int(cwnd) / growthThisRound
	if probeUpBytes < defaultMSS {
		probeUpBytes = defaultMSS
	}
	cycle.ProbeUpBytes = &probeUpBytes
}

func (p *ProbeBwState) probeInflightHighUpward(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
) {
	if params.ProbeUpIgnoreInflightHi {
		// When inflight_hi is disabled in PROBE_UP, it increases when
		// the number of bytes delivered in a round is larger than inflight_hi.
		return
	}

	// TODO(quiche): probe_up_simplify_inflight_hi?
	if congestionEvent.PriorBytesInFlight < congestionEvent.PriorCwnd {
		// Not fully utilizing cwnd, so can't safely grow.
		return
	}

	if congestionEvent.PriorCwnd < model.InflightHi() {
		// Not fully using inflight_hi, so don't grow it.
		return
	}

	// Increase inflight_hi by the number of probe_up_bytes within probe_up_acked.
	cycle.ProbeUpAcked += int(congestionEvent.BytesAcked)

	if cycle.ProbeUpBytes != nil && cycle.ProbeUpAcked >= *cycle.ProbeUpBytes {
		delta := cycle.ProbeUpAcked / *cycle.ProbeUpBytes
		cycle.ProbeUpAcked -= *cycle.ProbeUpBytes * delta
		newInflightHi := model.InflightHi() + congestion.ByteCount(delta*defaultMSS)
		if newInflightHi > model.InflightHi() {
			model.SetInflightHi(newInflightHi)
		}
	}

	if congestionEvent.EndOfRoundTrip {
		p.raiseInflightHighSlope(cycle, congestionEvent.PriorCwnd)
	}
}

// OnExitQuiescence handles exiting from quiescence in PROBE_BW.
func (p *ProbeBwState) OnExitQuiescence(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	now monotime.Time,
	quiescenceStartTime monotime.Time,
	params *Params,
) Mode {
	model.PostponeMinRttTimestamp(now.Sub(quiescenceStartTime))
	return ModeProbeBw
}
