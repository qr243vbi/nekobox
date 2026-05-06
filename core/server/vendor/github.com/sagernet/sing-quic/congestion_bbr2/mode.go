// BBRv2 mode and cycle phase definitions
// src from: https://github.com/cloudflare/quiche/blob/master/quiche/src/recovery/gcongestion/bbr2/mode.rs

package congestion_bbr2

import (
	"time"

	"github.com/sagernet/quic-go/monotime"
)

// Mode represents the main BBRv2 state machine states.
type Mode int

const (
	ModeStartup Mode = iota
	ModeDrain
	ModeProbeBw
	ModeProbeRtt
)

func (m Mode) String() string {
	switch m {
	case ModeStartup:
		return "STARTUP"
	case ModeDrain:
		return "DRAIN"
	case ModeProbeBw:
		return "PROBE_BW"
	case ModeProbeRtt:
		return "PROBE_RTT"
	default:
		return "UNKNOWN"
	}
}

// CyclePhase represents the PROBE_BW sub-states.
type CyclePhase int

const (
	CyclePhaseNotStarted CyclePhase = iota
	CyclePhaseDown
	CyclePhaseCruise
	CyclePhaseRefill
	CyclePhaseUp
)

func (c CyclePhase) String() string {
	switch c {
	case CyclePhaseNotStarted:
		return "NOT_STARTED"
	case CyclePhaseDown:
		return "DOWN"
	case CyclePhaseCruise:
		return "CRUISE"
	case CyclePhaseRefill:
		return "REFILL"
	case CyclePhaseUp:
		return "UP"
	default:
		return "UNKNOWN"
	}
}

// Cycle tracks the PROBE_BW cycle state.
type Cycle struct {
	// Time when the cycle started.
	StartTime monotime.Time
	// Current phase within the cycle.
	Phase CyclePhase
	// Time when the current phase started.
	PhaseStartTime monotime.Time
	// Number of rounds in the current phase.
	RoundsInPhase int
	// Number of rounds since the last probe.
	RoundsSinceProbe int
	// Randomized probe wait time.
	ProbeWaitTime time.Duration
	// Number of rounds probing up.
	ProbeUpRounds int
	// Bytes sent while probing up. nil means not set.
	ProbeUpBytes *int
	// Bytes acked while probing up.
	ProbeUpAcked int
	// Tracks if app became limited since inflight_hi limited in this PROBE_UP cycle.
	ProbeUpAppLimitedSinceInflightHiLimited bool
	// Whether the current sample is from probing.
	IsSampleFromProbing bool
	// Whether the last cycle probed too high.
	LastCycleProbedTooHigh bool
	// Whether the last cycle stopped a risky probe.
	LastCycleStoppedRiskyProbe bool
	// Whether max bandwidth filter window has advanced in this cycle.
	hasAdvancedMaxBw bool
}

// NewCycle creates a new Cycle in the initial state.
func NewCycle() *Cycle {
	return &Cycle{
		Phase: CyclePhaseNotStarted,
	}
}

// AdaptUpperBoundsResult represents the result of adapting upper bounds.
type AdaptUpperBoundsResult int

const (
	AdaptUpperBoundsNotAdaptedInvalidSample AdaptUpperBoundsResult = iota
	AdaptUpperBoundsNotAdaptedInflightHighNotSet
	AdaptUpperBoundsAdaptedProbedTooHigh
	AdaptUpperBoundsAdaptedOk
)

// Limits represents min/max bounds for a value.
type Limits struct {
	Lo int
	Hi int
}

// Min returns the lower bound.
func (l Limits) Min() int {
	return l.Lo
}

// ApplyLimits clamps val to the [Lo, Hi] range.
func (l Limits) ApplyLimits(val int) int {
	if val < l.Lo {
		return l.Lo
	}
	if val > l.Hi {
		return l.Hi
	}
	return val
}

// NoGreaterThan creates limits with Lo=0 and Hi=val.
func NoGreaterThan(val int) Limits {
	return Limits{Lo: 0, Hi: val}
}
