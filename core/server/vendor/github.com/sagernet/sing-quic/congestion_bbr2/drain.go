// BBRv2 DRAIN state implementation.
// DRAIN follows STARTUP and reduces the inflight bytes to the estimated BDP.
// src from: https://github.com/cloudflare/quiche/blob/master/quiche/src/recovery/gcongestion/bbr2/drain.rs

package congestion_bbr2

import (
	"github.com/sagernet/quic-go/congestion"
	"github.com/sagernet/quic-go/monotime"
)

// DrainState handles the DRAIN mode logic.
type DrainState struct{}

// IsProbingForBandwidth returns false as DRAIN is not probing.
func (d *DrainState) IsProbingForBandwidth() bool {
	return false
}

// Enter is called when entering DRAIN state.
func (d *DrainState) Enter(
	model *BBRv2NetworkModel,
	now monotime.Time,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
) {
	model.SetPacingGain(params.DrainPacingGain)
	// Only STARTUP can transition to DRAIN, both of them use the same cwnd gain.
	model.SetCwndGain(params.DrainCwndGain)
}

// Leave is called when leaving DRAIN state.
func (d *DrainState) Leave(
	model *BBRv2NetworkModel,
	now monotime.Time,
	congestionEvent *BBRv2CongestionEvent,
) {
	// No special leave logic for DRAIN.
}

// GetCwndLimits returns the cwnd limits for DRAIN.
func (d *DrainState) GetCwndLimits(model *BBRv2NetworkModel, params *Params) Limits {
	return Limits{
		Lo: 0,
		Hi: int(model.InflightLo()),
	}
}

// OnCongestionEvent handles a congestion event in DRAIN state.
// Returns the next mode and whether mode changed.
func (d *DrainState) OnCongestionEvent(
	model *BBRv2NetworkModel,
	eventTime monotime.Time,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
	cwnd congestion.ByteCount,
) (nextMode Mode, modeChanged bool) {
	model.SetPacingGain(params.DrainPacingGain)
	// Only STARTUP can transition to DRAIN, both of them use the same cwnd gain.
	model.SetCwndGain(params.DrainCwndGain)

	drainTarget := d.drainTarget(model)
	if congestionEvent.BytesInFlight <= drainTarget {
		return ModeProbeBw, true
	}

	return ModeDrain, false
}

// drainTarget returns the target inflight for draining.
func (d *DrainState) drainTarget(model *BBRv2NetworkModel) congestion.ByteCount {
	return model.BDP0()
}

// OnExitQuiescence handles exiting from quiescence in DRAIN.
func (d *DrainState) OnExitQuiescence(
	model *BBRv2NetworkModel,
	now monotime.Time,
	quiescenceStartTime monotime.Time,
	params *Params,
) Mode {
	return ModeDrain
}
