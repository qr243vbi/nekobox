// BBRv2 PROBE_RTT state implementation.
// PROBE_RTT is entered periodically to re-probe the minimum RTT.
// It reduces inflight to allow RTT samples without queuing delay.
// src from: https://github.com/cloudflare/quiche/blob/master/quiche/src/recovery/gcongestion/bbr2/probe_rtt.rs

package congestion_bbr2

import (
	"github.com/sagernet/quic-go/congestion"
	"github.com/sagernet/quic-go/monotime"
)

// ProbeRttState handles the PROBE_RTT mode logic.
type ProbeRttState struct {
	// Time when PROBE_RTT should exit. Nil if not yet set.
	exitTime *monotime.Time
}

// NewProbeRttState creates a new ProbeRttState.
func NewProbeRttState() *ProbeRttState {
	return &ProbeRttState{
		exitTime: nil,
	}
}

// IsProbingForBandwidth returns false as PROBE_RTT is not probing.
func (p *ProbeRttState) IsProbingForBandwidth() bool {
	return false
}

// Enter is called when entering PROBE_RTT state.
func (p *ProbeRttState) Enter(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	now monotime.Time,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
) {
	model.SetPacingGain(params.ProbeRttPacingGain)
	model.SetCwndGain(params.ProbeRttCwndGain)
	p.exitTime = nil
}

// Leave is called when leaving PROBE_RTT state.
func (p *ProbeRttState) Leave(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	now monotime.Time,
	congestionEvent *BBRv2CongestionEvent,
) {
	// No special leave logic for PROBE_RTT.
}

// GetCwndLimits returns the cwnd limits for PROBE_RTT.
func (p *ProbeRttState) GetCwndLimits(model *BBRv2NetworkModel, cycle *Cycle, params *Params) Limits {
	inflightUpperBound := model.InflightLo()
	hiWithHeadroom := model.InflightHiWithHeadroom()
	if hiWithHeadroom < inflightUpperBound {
		inflightUpperBound = hiWithHeadroom
	}

	target := p.inflightTarget(model, params)
	if target < inflightUpperBound {
		return NoGreaterThan(int(target))
	}
	return NoGreaterThan(int(inflightUpperBound))
}

// inflightTarget returns the target inflight for PROBE_RTT.
func (p *ProbeRttState) inflightTarget(model *BBRv2NetworkModel, params *Params) congestion.ByteCount {
	return model.BDP(model.MaxBandwidth(), params.ProbeRttInflightTargetBdpFraction)
}

// OnCongestionEvent handles a congestion event in PROBE_RTT state.
// Returns the next mode and whether mode changed.
func (p *ProbeRttState) OnCongestionEvent(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	eventTime monotime.Time,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
) (nextMode Mode, modeChanged bool) {
	if p.exitTime == nil {
		// Haven't set exit time yet
		if congestionEvent.BytesInFlight <= p.inflightTarget(model, params) {
			exitTime := congestionEvent.EventTime.Add(params.ProbeRttDuration)
			p.exitTime = &exitTime
		}
		return ModeProbeRtt, false
	}

	// Exit time is set, check if we should exit
	if congestionEvent.EventTime.After(*p.exitTime) {
		return ModeProbeBw, true
	}

	return ModeProbeRtt, false
}

// OnExitQuiescence handles exiting from quiescence in PROBE_RTT.
func (p *ProbeRttState) OnExitQuiescence(
	model *BBRv2NetworkModel,
	cycle *Cycle,
	now monotime.Time,
	quiescenceStartTime monotime.Time,
	params *Params,
) Mode {
	if p.exitTime == nil {
		// Exit time never set, go back to PROBE_BW
		return ModeProbeBw
	}

	if now.After(*p.exitTime) {
		// Exit time has passed, go back to PROBE_BW
		return ModeProbeBw
	}

	return ModeProbeRtt
}

// Reset resets the PROBE_RTT state for reuse.
func (p *ProbeRttState) Reset() {
	p.exitTime = nil
}
