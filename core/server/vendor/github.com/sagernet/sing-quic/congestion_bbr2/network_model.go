// BBRv2 network model implementation.
// Takes low level congestion signals (packets sent/acked/lost) as input and produces
// BBRv2 model parameters like inflight_(hi|lo), bandwidth_(hi|lo), bandwidth and rtt estimates.
// src from: https://github.com/cloudflare/quiche/blob/master/quiche/src/recovery/gcongestion/bbr2/network_model.rs

package congestion_bbr2

import (
	"math"
	"time"

	"github.com/sagernet/quic-go/congestion"
	"github.com/sagernet/quic-go/monotime"
)

const defaultMSS = 1300

// roundTripCounter tracks round trips based on packet numbers.
type roundTripCounter struct {
	roundTripCount int64
	lastSentPacket congestion.PacketNumber
	// The last sent packet number of the current round trip.
	endOfRoundTrip *congestion.PacketNumber
}

// onPacketSent must be called in ascending packet number order.
func (r *roundTripCounter) onPacketSent(packetNumber congestion.PacketNumber) {
	r.lastSentPacket = packetNumber
}

// onPacketsAcked returns whether a round trip has just completed.
func (r *roundTripCounter) onPacketsAcked(lastAckedPacket congestion.PacketNumber) bool {
	if r.endOfRoundTrip != nil && lastAckedPacket <= *r.endOfRoundTrip {
		return false
	}
	r.roundTripCount++
	r.endOfRoundTrip = &r.lastSentPacket
	return true
}

func (r *roundTripCounter) restartRound() {
	r.endOfRoundTrip = &r.lastSentPacket
}

// minRttFilter tracks the minimum RTT over time.
type minRttFilter struct {
	minRtt          time.Duration
	minRttTimestamp monotime.Time
}

func (f *minRttFilter) get() time.Duration {
	return f.minRtt
}

func (f *minRttFilter) getTimestamp() monotime.Time {
	return f.minRttTimestamp
}

func (f *minRttFilter) update(sampleRtt time.Duration, now monotime.Time) {
	if sampleRtt < f.minRtt {
		f.minRtt = sampleRtt
		f.minRttTimestamp = now
	}
}

func (f *minRttFilter) forceUpdate(sampleRtt time.Duration, now monotime.Time) {
	f.minRtt = sampleRtt
	f.minRttTimestamp = now
}

// maxBandwidthFilter tracks max bandwidth using a 2-element array.
type maxBandwidthFilter struct {
	maxBandwidth [2]Bandwidth
}

func (f *maxBandwidthFilter) get() Bandwidth {
	if f.maxBandwidth[0] > f.maxBandwidth[1] {
		return f.maxBandwidth[0]
	}
	return f.maxBandwidth[1]
}

func (f *maxBandwidthFilter) update(sample Bandwidth) {
	if sample > f.maxBandwidth[1] {
		f.maxBandwidth[1] = sample
	}
}

func (f *maxBandwidthFilter) advance() {
	if f.maxBandwidth[1] == 0 {
		return
	}
	f.maxBandwidth[0] = f.maxBandwidth[1]
	f.maxBandwidth[1] = 0
}

// BBRv2CongestionEvent holds information about a congestion event.
type BBRv2CongestionEvent struct {
	// Time of the event.
	EventTime monotime.Time
	// Whether this is the end of a round trip.
	EndOfRoundTrip bool
	// Send state of the last acked/lost packet.
	LastPacketSendState SendTimeState
	// Bytes acked in this event.
	BytesAcked congestion.ByteCount
	// Bytes lost in this event.
	BytesLost congestion.ByteCount
	// Bytes in flight before the event.
	PriorBytesInFlight congestion.ByteCount
	// Bytes in flight after the event.
	BytesInFlight congestion.ByteCount
	// Prior cwnd before the event.
	PriorCwnd congestion.ByteCount
	// Whether probing for bandwidth during this event.
	IsProbingForBandwidth bool
	// Sample max bandwidth from this event.
	SampleMaxBandwidth *Bandwidth
	// Sample min RTT from this event.
	SampleMinRtt *time.Duration
}

// BBRv2NetworkModel contains the BBRv2 network model state.
type BBRv2NetworkModel struct {
	roundTripCounter roundTripCounter
	// Bandwidth sampler provides BBR with the bandwidth measurements at individual points.
	bandwidthSampler *BandwidthSampler
	// The filter that tracks the maximum bandwidth over multiple recent round trips.
	maxBandwidthFilter maxBandwidthFilter
	minRttFilter       minRttFilter

	// Bytes lost in the current round. Updated once per congestion event.
	bytesLostInRound congestion.ByteCount
	// Number of loss marking events in the current round.
	lossEventsInRound int

	// A max of bytes delivered among all congestion events in the current round.
	maxBytesDeliveredInRound congestion.ByteCount

	// The minimum bytes in flight during this round.
	minBytesInFlightInRound congestion.ByteCount
	// True if sending was limited by inflight_hi anytime in the current round.
	inflightHiLimitedInRound bool

	// Max bandwidth in the current round. Updated once per congestion event.
	bandwidthLatest Bandwidth
	// Max bandwidth of recent rounds. Updated once per round.
	bandwidthLo      *Bandwidth
	priorBandwidthLo *Bandwidth

	// Max inflight in the current round. Updated once per congestion event.
	inflightLatest congestion.ByteCount
	// Max inflight of recent rounds. Updated once per round.
	inflightLo congestion.ByteCount
	inflightHi congestion.ByteCount

	cwndGain   float64
	pacingGain float64

	// Whether we are cwnd limited prior to the start of the current aggregation epoch.
	cwndLimitedBeforeAggregationEpoch bool

	// STARTUP-centric fields which experimentally used by PROBE_UP.
	fullBandwidthReached         bool
	fullBandwidthBaseline        Bandwidth
	roundsWithoutBandwidthGrowth int

	// Used by STARTUP and PROBE_UP to decide when to exit.
	roundsWithQueueing int

	// Determines whether app limited rounds with no bandwidth growth count
	// towards the rounds threshold to exit startup.
	ignoreAppLimitedForNoBandwidthGrowth bool

	params *Params
}

// NewBBRv2NetworkModel creates a new BBRv2 network model.
func NewBBRv2NetworkModel(params *Params, initialRtt time.Duration) *BBRv2NetworkModel {
	return &BBRv2NetworkModel{
		minBytesInFlightInRound:  math.MaxInt64,
		inflightHiLimitedInRound: false,
		bandwidthSampler: NewBandwidthSampler(
			RoundTripCount(params.InitialMaxAckHeightFilterWindow),
			params.EnableOverestimateAvoidance,
			params.ChooseA0PointFix,
		),
		roundTripCounter: roundTripCounter{
			roundTripCount: 0,
			lastSentPacket: 0,
			endOfRoundTrip: nil,
		},
		minRttFilter: minRttFilter{
			minRtt:          initialRtt,
			minRttTimestamp: monotime.Now(),
		},
		maxBandwidthFilter:                   maxBandwidthFilter{},
		cwndLimitedBeforeAggregationEpoch:    false,
		cwndGain:                             params.StartupCwndGain,
		pacingGain:                           params.StartupPacingGain,
		fullBandwidthReached:                 false,
		bytesLostInRound:                     0,
		lossEventsInRound:                    0,
		maxBytesDeliveredInRound:             0,
		bandwidthLatest:                      0,
		bandwidthLo:                          nil,
		priorBandwidthLo:                     nil,
		inflightLatest:                       0,
		inflightLo:                           math.MaxInt64,
		inflightHi:                           math.MaxInt64,
		fullBandwidthBaseline:                0,
		roundsWithoutBandwidthGrowth:         0,
		roundsWithQueueing:                   0,
		ignoreAppLimitedForNoBandwidthGrowth: params.IgnoreAppLimitedForNoBandwidthGrowth,
		params:                               params,
	}
}

// MaxAckHeight returns the maximum ack height.
func (m *BBRv2NetworkModel) MaxAckHeight() congestion.ByteCount {
	return m.bandwidthSampler.MaxAckHeight()
}

// BandwidthEstimate returns the estimated bandwidth.
func (m *BBRv2NetworkModel) BandwidthEstimate() Bandwidth {
	if m.bandwidthLo == nil {
		return m.MaxBandwidth()
	}
	if *m.bandwidthLo < m.MaxBandwidth() {
		return *m.bandwidthLo
	}
	return m.MaxBandwidth()
}

// BDP calculates the bandwidth-delay product with a gain factor.
func (m *BBRv2NetworkModel) BDP(bandwidth Bandwidth, gain float64) congestion.ByteCount {
	return bandwidth.Mul(gain).ToBytesPerPeriod(m.MinRtt())
}

// BDP1 calculates the BDP with gain = 1.0.
func (m *BBRv2NetworkModel) BDP1(bandwidth Bandwidth) congestion.ByteCount {
	return m.BDP(bandwidth, 1.0)
}

// BDP0 calculates the BDP using max bandwidth with gain = 1.0.
func (m *BBRv2NetworkModel) BDP0() congestion.ByteCount {
	return m.BDP1(m.MaxBandwidth())
}

// MinRtt returns the minimum RTT.
func (m *BBRv2NetworkModel) MinRtt() time.Duration {
	return m.minRttFilter.get()
}

// MinRttTimestamp returns the timestamp of the minimum RTT.
func (m *BBRv2NetworkModel) MinRttTimestamp() monotime.Time {
	return m.minRttFilter.getTimestamp()
}

// MaxBandwidth returns the maximum bandwidth.
func (m *BBRv2NetworkModel) MaxBandwidth() Bandwidth {
	return m.maxBandwidthFilter.get()
}

// OnPacketSent handles a sent packet.
func (m *BBRv2NetworkModel) OnPacketSent(
	sentTime monotime.Time,
	bytesInFlight congestion.ByteCount,
	packetNumber congestion.PacketNumber,
	bytes congestion.ByteCount,
	isRetransmittable bool,
) {
	// Updating the min here ensures a more realistic (0) value when flows exit quiescence.
	if bytesInFlight < m.minBytesInFlightInRound {
		m.minBytesInFlightInRound = bytesInFlight
	}

	if bytesInFlight+bytes >= m.inflightHi {
		m.inflightHiLimitedInRound = true
	}
	m.roundTripCounter.onPacketSent(packetNumber)

	m.bandwidthSampler.OnPacketSent(
		sentTime,
		packetNumber,
		bytes,
		bytesInFlight,
		isRetransmittable,
	)
}

// OnCongestionEventStart handles the start of a congestion event.
func (m *BBRv2NetworkModel) OnCongestionEventStart(
	ackedPackets []congestion.AckedPacketInfo,
	lostPackets []congestion.LostPacketInfo,
	congestionEvent *BBRv2CongestionEvent,
) {
	priorBytesAcked := m.TotalBytesAcked()
	priorBytesLost := m.TotalBytesLost()

	eventTime := congestionEvent.EventTime

	// Check for end of round trip
	if len(ackedPackets) > 0 {
		largestAcked := ackedPackets[len(ackedPackets)-1].PacketNumber
		congestionEvent.EndOfRoundTrip = m.roundTripCounter.onPacketsAcked(largestAcked)
	} else {
		congestionEvent.EndOfRoundTrip = false
	}

	// Get bandwidth sample
	bandwidthLoForSample := infBandwidth
	if m.bandwidthLo != nil {
		bandwidthLoForSample = *m.bandwidthLo
	}
	sample := m.bandwidthSampler.OnCongestionEvent(
		eventTime,
		ackedPackets,
		lostPackets,
		m.MaxBandwidth(),
		bandwidthLoForSample,
		RoundTripCount(m.roundTripCounter.roundTripCount),
	)

	if sample.ExtraAcked == 0 {
		m.cwndLimitedBeforeAggregationEpoch = congestionEvent.PriorBytesInFlight >= congestionEvent.PriorCwnd
	}

	if sample.LastPacketSendState.IsValid {
		congestionEvent.LastPacketSendState = sample.LastPacketSendState
	}

	// Avoid updating max_bandwidth_filter if a) this is a loss-only event,
	// or b) all packets in acked_packets did not generate valid samples.
	if priorBytesAcked != m.TotalBytesAcked() && sample.SampleMaxBandwidth > 0 {
		sampleMax := sample.SampleMaxBandwidth
		congestionEvent.SampleMaxBandwidth = &sampleMax
		if !sample.SampleIsAppLimited || sampleMax > m.MaxBandwidth() {
			m.maxBandwidthFilter.update(sampleMax)
		}
	}

	if sample.SampleRtt > 0 {
		rttSample := sample.SampleRtt
		congestionEvent.SampleMinRtt = &rttSample
		m.minRttFilter.update(rttSample, eventTime)
	}

	congestionEvent.BytesAcked = m.TotalBytesAcked() - priorBytesAcked
	congestionEvent.BytesLost = m.TotalBytesLost() - priorBytesLost

	bytesInFlight := congestionEvent.PriorBytesInFlight - congestionEvent.BytesAcked - congestionEvent.BytesLost
	if bytesInFlight < 0 {
		bytesInFlight = 0
	}
	congestionEvent.BytesInFlight = bytesInFlight

	if congestionEvent.BytesLost > 0 {
		m.bytesLostInRound += congestionEvent.BytesLost
		m.lossEventsInRound++
	}

	if congestionEvent.BytesAcked > 0 &&
		congestionEvent.LastPacketSendState.IsValid &&
		m.TotalBytesAcked() > congestionEvent.LastPacketSendState.TotalBytesAcked {
		bytesDelivered := m.TotalBytesAcked() - congestionEvent.LastPacketSendState.TotalBytesAcked
		if bytesDelivered > m.maxBytesDeliveredInRound {
			m.maxBytesDeliveredInRound = bytesDelivered
		}
	}

	if congestionEvent.BytesInFlight < m.minBytesInFlightInRound {
		m.minBytesInFlightInRound = congestionEvent.BytesInFlight
	}

	// bandwidth_latest and inflight_latest only increased within a round.
	if sample.SampleMaxBandwidth > m.bandwidthLatest {
		m.bandwidthLatest = sample.SampleMaxBandwidth
	}

	if sample.SampleMaxInflight > m.inflightLatest {
		m.inflightLatest = sample.SampleMaxInflight
	}

	// Adapt lower bounds (bandwidth_lo and inflight_lo).
	m.adaptLowerBounds(congestionEvent)

	if !congestionEvent.EndOfRoundTrip {
		return
	}

	if sample.SampleMaxBandwidth > 0 {
		m.bandwidthLatest = sample.SampleMaxBandwidth
	}

	if sample.SampleMaxInflight > 0 {
		m.inflightLatest = sample.SampleMaxInflight
	}
}

// OnPacketNeutered handles a neutered packet.
func (m *BBRv2NetworkModel) OnPacketNeutered(packetNumber congestion.PacketNumber) {
	m.bandwidthSampler.OnPacketNeutered(packetNumber)
}

func (m *BBRv2NetworkModel) adaptLowerBounds(congestionEvent *BBRv2CongestionEvent) {
	params := m.params

	if params.BwLoMode == BwLoModeDefault {
		if !congestionEvent.EndOfRoundTrip || congestionEvent.IsProbingForBandwidth {
			return
		}

		if m.bytesLostInRound > 0 {
			if m.bandwidthLo == nil {
				bw := m.MaxBandwidth()
				m.bandwidthLo = &bw
			}

			newBandwidthLo := m.bandwidthLatest
			reduced := (*m.bandwidthLo).Mul(1.0 - params.Beta)
			if reduced > newBandwidthLo {
				newBandwidthLo = reduced
			}
			m.bandwidthLo = &newBandwidthLo

			if m.inflightLo == math.MaxInt64 {
				m.inflightLo = congestionEvent.PriorCwnd
			}

			inflightLoNew := congestion.ByteCount(float64(m.inflightLo) * (1.0 - params.Beta))
			if m.inflightLatest > inflightLoNew {
				m.inflightLo = m.inflightLatest
			} else {
				m.inflightLo = inflightLoNew
			}
		}
		return
	}

	if congestionEvent.BytesLost == 0 {
		return
	}

	// Ignore losses from packets sent when probing for more bandwidth in
	// STARTUP or PROBE_UP when they're lost in DRAIN or PROBE_DOWN.
	if m.pacingGain < 1.0 {
		return
	}

	// Decrease bandwidth_lo whenever there is loss.
	// Set bandwidth_lo if it is not yet set.
	if m.bandwidthLo == nil {
		bw := m.MaxBandwidth()
		m.bandwidthLo = &bw
	}

	// Save bandwidth_lo if it hasn't already been saved.
	if m.priorBandwidthLo == nil {
		m.priorBandwidthLo = m.bandwidthLo
	}

	switch params.BwLoMode {
	case BwLoModeMinRttReduction:
		reduction := BandwidthFromBytesAndTimeDelta(congestionEvent.BytesLost, m.MinRtt())
		if *m.bandwidthLo > reduction {
			newBw := *m.bandwidthLo - reduction
			m.bandwidthLo = &newBw
		} else {
			zero := Bandwidth(0)
			m.bandwidthLo = &zero
		}

	case BwLoModeInflightReduction:
		// Use a max of BDP and inflight to avoid starving app-limited flows.
		effectiveInflight := m.BDP0()
		if congestionEvent.PriorBytesInFlight > effectiveInflight {
			effectiveInflight = congestionEvent.PriorBytesInFlight
		}
		factor := float64(effectiveInflight-congestionEvent.BytesLost) / float64(effectiveInflight)
		if factor < 0 {
			factor = 0
		}
		newBw := (*m.bandwidthLo).Mul(factor)
		m.bandwidthLo = &newBw

	case BwLoModeCwndReduction:
		factor := float64(congestionEvent.PriorCwnd-congestionEvent.BytesLost) / float64(congestionEvent.PriorCwnd)
		if factor < 0 {
			factor = 0
		}
		newBw := (*m.bandwidthLo).Mul(factor)
		m.bandwidthLo = &newBw
	}

	lastBandwidth := m.bandwidthLatest
	// sample_max_bandwidth will be nil if the loss is triggered by a timer expiring.
	if congestionEvent.SampleMaxBandwidth != nil {
		lastBandwidth = *congestionEvent.SampleMaxBandwidth
	}

	if m.pacingGain > params.FullBwThreshold {
		// In STARTUP, pacing_gain is applied to bandwidth_lo in update_pacing_rate,
		// so this backs that multiplication out to allow the pacing rate to decrease,
		// but not below last_bandwidth * full_bw_threshold.
		minBw := lastBandwidth.Mul(params.FullBwThreshold / m.pacingGain)
		if *m.bandwidthLo < minBw {
			m.bandwidthLo = &minBw
		}
	} else {
		// Ensure bandwidth_lo isn't lower than last_bandwidth.
		if *m.bandwidthLo < lastBandwidth {
			m.bandwidthLo = &lastBandwidth
		}
	}

	// If it's the end of the round, ensure bandwidth_lo doesn't decrease more than beta.
	if congestionEvent.EndOfRoundTrip && m.priorBandwidthLo != nil {
		minBw := (*m.priorBandwidthLo).Mul(1.0 - params.Beta)
		if *m.bandwidthLo < minBw {
			m.bandwidthLo = &minBw
		}
		m.priorBandwidthLo = nil
	}
	// These modes ignore inflight_lo as well.
}

// OnCongestionEventFinish handles the end of a congestion event.
func (m *BBRv2NetworkModel) OnCongestionEventFinish(
	leastUnackedPacket congestion.PacketNumber,
	congestionEvent *BBRv2CongestionEvent,
) {
	if congestionEvent.EndOfRoundTrip {
		m.onNewRound()
	}

	m.bandwidthSampler.RemoveObsoletePackets(leastUnackedPacket)
}

// MaybeExpireMinRtt checks if min RTT should be expired and returns true if it was.
func (m *BBRv2NetworkModel) MaybeExpireMinRtt(congestionEvent *BBRv2CongestionEvent) bool {
	if congestionEvent.SampleMinRtt == nil {
		return false
	}

	if congestionEvent.EventTime.Sub(m.minRttFilter.minRttTimestamp) < m.params.ProbeRttPeriod {
		return false
	}

	m.minRttFilter.forceUpdate(*congestionEvent.SampleMinRtt, congestionEvent.EventTime)
	return true
}

// IsInflightTooHigh checks if the inflight is too high based on loss.
func (m *BBRv2NetworkModel) IsInflightTooHigh(
	congestionEvent *BBRv2CongestionEvent,
	maxLossEvents int,
) bool {
	sendState := &congestionEvent.LastPacketSendState

	if !sendState.IsValid {
		// Not enough information.
		return false
	}

	if m.lossEventsInRound < maxLossEvents {
		return false
	}

	inflightAtSend := sendState.BytesInFlight
	bytesLostInRound := m.bytesLostInRound

	if inflightAtSend > 0 && bytesLostInRound > 0 {
		lostInRoundThreshold := congestion.ByteCount(float64(inflightAtSend) * m.params.LossThreshold)
		if bytesLostInRound > lostInRoundThreshold {
			return true
		}
	}

	return false
}

// RestartRoundEarly restarts the round early.
func (m *BBRv2NetworkModel) RestartRoundEarly() {
	m.onNewRound()
	m.roundTripCounter.restartRound()
	m.roundsWithQueueing = 0
}

func (m *BBRv2NetworkModel) onNewRound() {
	m.bytesLostInRound = 0
	m.lossEventsInRound = 0
	m.maxBytesDeliveredInRound = 0
	m.minBytesInFlightInRound = math.MaxInt64
	m.inflightHiLimitedInRound = false
}

// HasBandwidthGrowth checks if bandwidth has grown sufficiently.
func (m *BBRv2NetworkModel) HasBandwidthGrowth(congestionEvent *BBRv2CongestionEvent) bool {
	threshold := m.fullBandwidthBaseline.Mul(m.params.FullBwThreshold)

	if m.MaxBandwidth() >= threshold {
		m.fullBandwidthBaseline = m.MaxBandwidth()
		m.roundsWithoutBandwidthGrowth = 0
		return true
	}

	if !congestionEvent.LastPacketSendState.IsValid {
		// last_packet_send_state not available because the
		// congestion event did not contain any non-ACK frames.
		return false
	}

	ignoreRound := m.ignoreAppLimitedForNoBandwidthGrowth &&
		congestionEvent.LastPacketSendState.IsAppLimited

	if !ignoreRound {
		m.roundsWithoutBandwidthGrowth++
	}

	// full_bandwidth_reached is only set to true when not app-limited
	if m.roundsWithoutBandwidthGrowth >= m.params.StartupFullBwRounds &&
		!congestionEvent.LastPacketSendState.IsAppLimited {
		m.fullBandwidthReached = true
	}

	return false
}

// QueueingThresholdExtraBytes returns extra bytes for queueing threshold.
func (m *BBRv2NetworkModel) QueueingThresholdExtraBytes() congestion.ByteCount {
	return 2 * defaultMSS
}

// CheckPersistentQueue checks for persistent queueing.
func (m *BBRv2NetworkModel) CheckPersistentQueue(targetGain float64) {
	target := m.BDP(m.MaxBandwidth(), targetGain)
	minTarget := m.BDP0() + m.QueueingThresholdExtraBytes()
	if minTarget > target {
		target = minTarget
	}

	if m.minBytesInFlightInRound < target {
		m.roundsWithQueueing = 0
		return
	}

	m.roundsWithQueueing++
	if m.params.MaxStartupQueueRounds > 0 && m.roundsWithQueueing >= m.params.MaxStartupQueueRounds {
		m.fullBandwidthReached = true
	}
}

// MaxBytesDeliveredInRound returns the max bytes delivered in the current round.
func (m *BBRv2NetworkModel) MaxBytesDeliveredInRound() congestion.ByteCount {
	return m.maxBytesDeliveredInRound
}

// TotalBytesAcked returns total bytes acked.
func (m *BBRv2NetworkModel) TotalBytesAcked() congestion.ByteCount {
	return m.bandwidthSampler.TotalBytesAcked()
}

// TotalBytesLost returns total bytes lost.
func (m *BBRv2NetworkModel) TotalBytesLost() congestion.ByteCount {
	return m.bandwidthSampler.TotalBytesLost()
}

// RoundTripCount returns the current round trip count.
func (m *BBRv2NetworkModel) RoundTripCount() int64 {
	return m.roundTripCounter.roundTripCount
}

// FullBandwidthReached returns whether full bandwidth has been reached.
func (m *BBRv2NetworkModel) FullBandwidthReached() bool {
	return m.fullBandwidthReached
}

// SetFullBandwidthReached sets the full bandwidth reached flag.
func (m *BBRv2NetworkModel) SetFullBandwidthReached() {
	m.fullBandwidthReached = true
}

// PacingGain returns the current pacing gain.
func (m *BBRv2NetworkModel) PacingGain() float64 {
	return m.pacingGain
}

// SetPacingGain sets the pacing gain.
func (m *BBRv2NetworkModel) SetPacingGain(pacingGain float64) {
	m.pacingGain = pacingGain
}

// CwndGain returns the current cwnd gain.
func (m *BBRv2NetworkModel) CwndGain() float64 {
	return m.cwndGain
}

// SetCwndGain sets the cwnd gain.
func (m *BBRv2NetworkModel) SetCwndGain(cwndGain float64) {
	m.cwndGain = cwndGain
}

// InflightHi returns the inflight_hi value.
func (m *BBRv2NetworkModel) InflightHi() congestion.ByteCount {
	return m.inflightHi
}

// InflightHiWithHeadroom returns inflight_hi with headroom subtracted.
func (m *BBRv2NetworkModel) InflightHiWithHeadroom() congestion.ByteCount {
	headroom := congestion.ByteCount(float64(m.inflightHi) * m.params.InflightHiHeadroom)
	if m.inflightHi > headroom {
		return m.inflightHi - headroom
	}
	return 0
}

// SetInflightHi sets the inflight_hi value.
func (m *BBRv2NetworkModel) SetInflightHi(newInflightHi congestion.ByteCount) {
	m.inflightHi = newInflightHi
}

// InflightHiDefault returns the default (unset) inflight_hi value.
func (m *BBRv2NetworkModel) InflightHiDefault() congestion.ByteCount {
	return math.MaxInt64
}

// InflightLo returns the inflight_lo value.
func (m *BBRv2NetworkModel) InflightLo() congestion.ByteCount {
	return m.inflightLo
}

// ClearInflightLo clears inflightLo.
func (m *BBRv2NetworkModel) ClearInflightLo() {
	m.inflightLo = math.MaxInt64
}

// CapInflightLo caps inflight_lo at the given value.
func (m *BBRv2NetworkModel) CapInflightLo(cap congestion.ByteCount) {
	if m.inflightLo != math.MaxInt64 {
		if cap < m.inflightLo {
			m.inflightLo = cap
		}
	}
}

// BandwidthLo returns the bandwidth_lo value (infinite if not set).
func (m *BBRv2NetworkModel) BandwidthLo() Bandwidth {
	if m.bandwidthLo == nil {
		return infBandwidth
	}
	return *m.bandwidthLo
}

// ClearBandwidthLo clears bandwidth_lo.
func (m *BBRv2NetworkModel) ClearBandwidthLo() {
	m.bandwidthLo = nil
}

// AdvanceMaxBandwidthFilter advances the max bandwidth filter.
func (m *BBRv2NetworkModel) AdvanceMaxBandwidthFilter() {
	m.maxBandwidthFilter.advance()
}

// PostponeMinRttTimestamp postpones the min RTT timestamp.
func (m *BBRv2NetworkModel) PostponeMinRttTimestamp(duration time.Duration) {
	newTimestamp := m.minRttFilter.minRttTimestamp.Add(duration)
	m.minRttFilter.forceUpdate(m.MinRtt(), newTimestamp)
}

// OnAppLimited marks the connection as app limited.
func (m *BBRv2NetworkModel) OnAppLimited() {
	m.bandwidthSampler.OnAppLimited()
}

// LossEventsInRound returns the number of loss events in the current round.
func (m *BBRv2NetworkModel) LossEventsInRound() int {
	return m.lossEventsInRound
}

// RoundsWithQueueing returns the number of rounds with queueing.
func (m *BBRv2NetworkModel) RoundsWithQueueing() int {
	return m.roundsWithQueueing
}

// IsAppLimited returns whether the sampler is app limited.
func (m *BBRv2NetworkModel) IsAppLimited() bool {
	return m.bandwidthSampler.IsAppLimited()
}

// BandwidthLatest returns the latest bandwidth sample.
func (m *BBRv2NetworkModel) BandwidthLatest() Bandwidth {
	return m.bandwidthLatest
}

// InflightLatest returns the latest inflight sample.
func (m *BBRv2NetworkModel) InflightLatest() congestion.ByteCount {
	return m.inflightLatest
}

// BytesLostInRound returns bytes lost in the current round.
func (m *BBRv2NetworkModel) BytesLostInRound() congestion.ByteCount {
	return m.bytesLostInRound
}

// MinBytesInFlightInRound returns the minimum bytes in flight in the current round.
func (m *BBRv2NetworkModel) MinBytesInFlightInRound() congestion.ByteCount {
	return m.minBytesInFlightInRound
}

// InflightHiLimitedInRound returns whether inflight was limited by inflight_hi in the current round.
func (m *BBRv2NetworkModel) InflightHiLimitedInRound() bool {
	return m.inflightHiLimitedInRound
}

// CwndLimitedBeforeAggregationEpoch returns whether cwnd was limited before aggregation epoch.
func (m *BBRv2NetworkModel) CwndLimitedBeforeAggregationEpoch() bool {
	return m.cwndLimitedBeforeAggregationEpoch
}

// CleanupObsoletePackets removes packet state data for packets that have been acked or lost.
// This prevents memory leaks in the bandwidth sampler.
func (m *BBRv2NetworkModel) CleanupObsoletePackets(leastUnacked congestion.PacketNumber) {
	m.bandwidthSampler.RemoveObsoletePackets(leastUnacked)
}
