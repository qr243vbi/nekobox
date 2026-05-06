// BBRv2 parameters
// src from: https://github.com/cloudflare/quiche/blob/master/quiche/src/recovery/gcongestion/bbr2.rs

package congestion_bbr2

import "time"

// BwLoMode determines how bandwidth_lo is reduced when losses occur.
type BwLoMode int

const (
	BwLoModeDefault BwLoMode = iota
	BwLoModeMinRttReduction
	BwLoModeInflightReduction
	BwLoModeCwndReduction
)

// Params contains BBRv2 tunable parameters.
type Params struct {
	// STARTUP parameters
	StartupCwndGain       float64
	StartupPacingGain     float64
	FullBwThreshold       float64
	StartupFullBwRounds   int
	MaxStartupQueueRounds int
	StartupFullLossCount  int

	// DRAIN parameters
	DrainCwndGain   float64
	DrainPacingGain float64

	// PROBE_BW parameters
	ProbeBwProbeMaxRounds      int
	EnableRenoCoexistence      bool
	ProbeBwProbeRenoGain       float64
	ProbeBwProbeBaseDuration   time.Duration
	ProbeBwFullLossCount       int
	ProbeBwProbeUpPacingGain   float64
	ProbeBwProbeDownPacingGain float64
	ProbeBwDefaultPacingGain   float64
	ProbeBwCwndGain            float64
	ProbeBwUpCwndGain          float64

	// PROBE_UP parameters
	ProbeUpIgnoreInflightHi bool
	// TODO(quiche): ProbeUpSimplifyInflightHi is not implemented in quiche yet.
	MaxProbeUpQueueRounds int

	// PROBE_RTT parameters
	ProbeRttInflightTargetBdpFraction float64
	ProbeRttPeriod                    time.Duration
	ProbeRttDuration                  time.Duration
	ProbeRttPacingGain                float64
	ProbeRttCwndGain                  float64

	// General parameters
	InitialMaxAckHeightFilterWindow int
	InflightHiHeadroom              float64
	LossThreshold                   float64
	Beta                            float64

	// Experimental flags
	AddAckHeightToQueueingThreshold             bool
	AvoidUnnecessaryProbeRtt                    bool
	LimitInflightHiByMaxDelivered               bool
	StartupLossExitUseMaxDeliveredForInflightHi bool
	UseBytesDeliveredForInflightHi              bool
	DecreaseStartupPacingAtEndOfRound           bool
	EnableOverestimateAvoidance                 bool
	ChooseA0PointFix                            bool
	BwLoMode                                    BwLoMode
	IgnoreAppLimitedForNoBandwidthGrowth        bool
	InitialPacingRateBytesPerSecond             *uint64
	ScalePacingRateByMss                        bool
	DisableProbeDownEarlyExit                   bool
}

// DefaultParams returns the default BBRv2 parameters.
func DefaultParams() *Params {
	return &Params{
		// STARTUP
		StartupCwndGain:       2.0,
		StartupPacingGain:     2.773,
		FullBwThreshold:       1.25,
		StartupFullBwRounds:   3,
		MaxStartupQueueRounds: 0,
		StartupFullLossCount:  8,

		// DRAIN
		DrainCwndGain:   2.0,
		DrainPacingGain: 1.0 / 2.885,

		// PROBE_BW
		ProbeBwProbeMaxRounds:      63,
		EnableRenoCoexistence:      true,
		ProbeBwProbeRenoGain:       1.0,
		ProbeBwProbeBaseDuration:   2000 * time.Millisecond,
		ProbeBwFullLossCount:       2,
		ProbeBwProbeUpPacingGain:   1.25,
		ProbeBwProbeDownPacingGain: 0.9, // BBRv3
		ProbeBwDefaultPacingGain:   1.0,
		ProbeBwCwndGain:            2.25, // BBRv3
		ProbeBwUpCwndGain:          2.25, // BBRv3

		// PROBE_UP
		ProbeUpIgnoreInflightHi: false,
		MaxProbeUpQueueRounds:   2,

		// PROBE_RTT
		ProbeRttInflightTargetBdpFraction: 0.5,
		ProbeRttPeriod:                    10000 * time.Millisecond,
		ProbeRttDuration:                  200 * time.Millisecond,
		ProbeRttPacingGain:                1.0,
		ProbeRttCwndGain:                  1.0,

		// General
		InitialMaxAckHeightFilterWindow: 10,
		InflightHiHeadroom:              0.15,
		LossThreshold:                   0.015,
		Beta:                            0.3,

		// Experimental flags
		AddAckHeightToQueueingThreshold:             false,
		AvoidUnnecessaryProbeRtt:                    true,
		LimitInflightHiByMaxDelivered:               true,
		StartupLossExitUseMaxDeliveredForInflightHi: true,
		UseBytesDeliveredForInflightHi:              true,
		DecreaseStartupPacingAtEndOfRound:           true,
		EnableOverestimateAvoidance:                 true,
		ChooseA0PointFix:                            false,
		BwLoMode:                                    BwLoModeInflightReduction,
		IgnoreAppLimitedForNoBandwidthGrowth:        false,
		InitialPacingRateBytesPerSecond:             nil,
		ScalePacingRateByMss:                        false,
		DisableProbeDownEarlyExit:                   false,
	}
}

// AggressiveParams returns BBRv2 parameters tuned for aggressive bandwidth acquisition.
// This increases bandwidth share (~58% vs ~50%) with moderate latency impact (~30ms vs ~22ms).
func AggressiveParams() *Params {
	params := DefaultParams()
	params.ProbeBwProbeUpPacingGain = 1.35   // Default: 1.25
	params.ProbeBwProbeDownPacingGain = 0.85 // Default: 0.9
	params.InflightHiHeadroom = 0.10         // Default: 0.15
	return params
}
