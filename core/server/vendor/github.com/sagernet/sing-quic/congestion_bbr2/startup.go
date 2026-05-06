// BBRv2 STARTUP state implementation.
// STARTUP is the initial state where BBR exponentially increases bandwidth to fill the pipe.
// src from: https://github.com/cloudflare/quiche/blob/master/quiche/src/recovery/gcongestion/bbr2/startup.rs

package congestion_bbr2

import (
	"github.com/sagernet/quic-go/congestion"
	"github.com/sagernet/quic-go/monotime"
)

// StartupState handles the STARTUP mode logic.
// Note: Unlike a previous implementation, we do NOT dynamically adjust pacing_gain
// in STARTUP. The decrease_startup_pacing_at_end_of_round parameter is handled
// in BBR2Sender.updatePacingRate(), matching the quiche implementation.
type StartupState struct{}

// IsProbingForBandwidth returns true as STARTUP is always probing.
func (s *StartupState) IsProbingForBandwidth() bool {
	return true
}

// Enter is called when entering STARTUP state.
// Note: STARTUP is the initial state, so enter() should not normally be called.
func (s *StartupState) Enter(
	model *BBRv2NetworkModel,
	now monotime.Time,
	congestionEvent *BBRv2CongestionEvent,
) {
	// STARTUP is the initial state, no special entry logic needed.
}

// Leave is called when leaving STARTUP state.
func (s *StartupState) Leave(
	model *BBRv2NetworkModel,
	now monotime.Time,
	congestionEvent *BBRv2CongestionEvent,
) {
	// Clear bandwidth_lo if it's set during STARTUP.
	model.ClearBandwidthLo()
}

// GetCwndLimits returns the cwnd limits for STARTUP.
func (s *StartupState) GetCwndLimits(model *BBRv2NetworkModel, params *Params) Limits {
	return Limits{
		Lo: 0,
		Hi: int(model.InflightLo()),
	}
}

// OnCongestionEvent handles a congestion event in STARTUP state.
// Returns the next mode and whether mode changed.
func (s *StartupState) OnCongestionEvent(
	model *BBRv2NetworkModel,
	eventTime monotime.Time,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
	cwnd congestion.ByteCount,
) (nextMode Mode, modeChanged bool) {
	if model.FullBandwidthReached() {
		return ModeDrain, true
	}

	if !congestionEvent.EndOfRoundTrip {
		return ModeStartup, false
	}

	hasBandwidthGrowth := model.HasBandwidthGrowth(congestionEvent)

	// Check for persistent queue if configured.
	checkPersistentQueue := params.MaxStartupQueueRounds > 0 && !hasBandwidthGrowth
	if checkPersistentQueue {
		// 1.75 is less than the 2x CWND gain, but substantially more than
		// 1.25x, the minimum bandwidth increase expected during STARTUP.
		model.CheckPersistentQueue(1.75)
	}

	// TCP BBR always exits upon excessive losses. QUIC BBRv1 does not exit
	// upon excessive losses, if enough bandwidth growth is observed or if the
	// sample was app limited.
	checkForExcessiveLoss := !congestionEvent.LastPacketSendState.IsAppLimited &&
		!hasBandwidthGrowth &&
		// check for excessive loss only if not exiting for other reasons
		!model.FullBandwidthReached()

	if checkForExcessiveLoss {
		s.checkExcessiveLosses(model, congestionEvent, params)
	}

	// Note: decrease_startup_pacing_at_end_of_round is handled in
	// BBR2Sender.updatePacingRate(), not here. This matches quiche behavior.

	if model.FullBandwidthReached() {
		return ModeDrain, true
	}

	return ModeStartup, false
}

// checkExcessiveLosses checks if losses are too high and sets inflight_hi if so.
func (s *StartupState) checkExcessiveLosses(
	model *BBRv2NetworkModel,
	congestionEvent *BBRv2CongestionEvent,
	params *Params,
) {
	// At the end of a round trip. Check if loss is too high in this round.
	if model.IsInflightTooHigh(congestionEvent, params.StartupFullLossCount) {
		newInflightHi := model.BDP0()

		if params.StartupLossExitUseMaxDeliveredForInflightHi {
			if model.MaxBytesDeliveredInRound() > newInflightHi {
				newInflightHi = model.MaxBytesDeliveredInRound()
			}
		}

		model.SetInflightHi(newInflightHi)
		model.SetFullBandwidthReached()
	}
}

// OnExitQuiescence handles exiting from quiescence in STARTUP.
func (s *StartupState) OnExitQuiescence(
	model *BBRv2NetworkModel,
	now monotime.Time,
	quiescenceStartTime monotime.Time,
	params *Params,
) Mode {
	return ModeStartup
}
