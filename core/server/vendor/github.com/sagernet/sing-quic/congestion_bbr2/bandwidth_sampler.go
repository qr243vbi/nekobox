// BBRv2 bandwidth sampler implementation.
// Tracks sent packets and calculates bandwidth samples from acknowledgements.
// src from: https://github.com/cloudflare/quiche/blob/master/quiche/src/recovery/gcongestion/bbr/bandwidth_sampler.rs

package congestion_bbr2

import (
	"time"

	"github.com/sagernet/quic-go/congestion"
	"github.com/sagernet/quic-go/monotime"
)

const (
	defaultConnectionStateMapQueueSize = 64
	invalidPacketNumber                = congestion.PacketNumber(0) - 1
)

// SendTimeState represents the connection state at the time a packet was sent.
type SendTimeState struct {
	// Whether this state is valid.
	IsValid bool
	// Whether the sender was app limited at the time the packet was sent.
	IsAppLimited bool
	// Total bytes sent at the time the packet was sent (includes the packet itself).
	TotalBytesSent congestion.ByteCount
	// Total bytes acked at the time the packet was sent.
	TotalBytesAcked congestion.ByteCount
	// Total bytes lost at the time the packet was sent.
	TotalBytesLost congestion.ByteCount
	// Bytes in flight at the time the packet was sent (includes the packet itself).
	BytesInFlight congestion.ByteCount
}

// CongestionEventSample holds the result of processing a congestion event.
type CongestionEventSample struct {
	// Maximum bandwidth sample from all acked packets.
	SampleMaxBandwidth Bandwidth
	// Whether SampleMaxBandwidth is from an app-limited sample.
	SampleIsAppLimited bool
	// Minimum RTT sample from all acked packets.
	SampleRtt time.Duration
	// Maximum bytes in flight when packets were in flight.
	SampleMaxInflight congestion.ByteCount
	// Send state of the largest acked or lost packet.
	LastPacketSendState SendTimeState
	// Extra bytes acked beyond expected bandwidth.
	ExtraAcked congestion.ByteCount
}

// connectionStateOnSentPacket represents the state when a packet was sent.
type connectionStateOnSentPacket struct {
	// Time at which the packet was sent.
	sentTime monotime.Time
	// Size of the packet.
	size congestion.ByteCount
	// Total bytes sent at the last acked packet when this packet was sent.
	totalBytesSentAtLastAckedPacket congestion.ByteCount
	// Sent time of the last acked packet when this packet was sent.
	lastAckedPacketSentTime monotime.Time
	// Ack time of the last acked packet when this packet was sent.
	lastAckedPacketAckTime monotime.Time
	// Send time state when this packet was sent.
	sendTimeState SendTimeState
}

// ackPoint represents a point on the ack line.
type ackPoint struct {
	ackTime         monotime.Time
	totalBytesAcked congestion.ByteCount
}

// recentAckPoints maintains the most recent 2 ack points at distinct times.
type recentAckPoints struct {
	ackPoints [2]*ackPoint
}

func (r *recentAckPoints) update(ackTime monotime.Time, totalBytesAcked congestion.ByteCount) {
	// Always shift: [1] -> [0], then create new point at [1]
	// This matches the quiche Rust implementation which unconditionally shifts.
	r.ackPoints[0] = r.ackPoints[1]
	r.ackPoints[1] = &ackPoint{
		ackTime:         ackTime,
		totalBytesAcked: totalBytesAcked,
	}
}

func (r *recentAckPoints) clear() {
	r.ackPoints[0] = nil
	r.ackPoints[1] = nil
}

func (r *recentAckPoints) mostRecent() *ackPoint {
	return r.ackPoints[1]
}

func (r *recentAckPoints) lessRecentPoint(chooseA0PointFix bool) *ackPoint {
	if chooseA0PointFix {
		if r.ackPoints[0] != nil && r.ackPoints[0].totalBytesAcked > 0 {
			return r.ackPoints[0]
		}
		return r.ackPoints[1]
	}
	if r.ackPoints[0] != nil {
		return r.ackPoints[0]
	}
	return r.ackPoints[1]
}

// extraAckedSample tracks extra bytes acked in an aggregation epoch.
type extraAckedSample struct {
	extraAcked congestion.ByteCount
	bytesAcked congestion.ByteCount // Total bytes acked in this aggregation epoch
	timeDelta  time.Duration        // Duration of this aggregation epoch
	round      RoundTripCount
}

// extraAckedFilter tracks max extra acked over a window.
type extraAckedFilter struct {
	windowLength RoundTripCount
	estimates    [3]*extraAckedSample
}

func (f *extraAckedFilter) getBest() *extraAckedSample {
	return f.estimates[0]
}

func (f *extraAckedFilter) getSecondBest() *extraAckedSample {
	return f.estimates[1]
}

func (f *extraAckedFilter) getThirdBest() *extraAckedSample {
	return f.estimates[2]
}

func (f *extraAckedFilter) clear() {
	f.estimates[0] = nil
	f.estimates[1] = nil
	f.estimates[2] = nil
}

func (f *extraAckedFilter) update(extraAcked, bytesAcked congestion.ByteCount, timeDelta time.Duration, round RoundTripCount) {
	s := &extraAckedSample{
		extraAcked: extraAcked,
		bytesAcked: bytesAcked,
		timeDelta:  timeDelta,
		round:      round,
	}

	if f.estimates[0] == nil || f.estimates[2] == nil ||
		extraAcked > f.estimates[0].extraAcked ||
		round-f.estimates[2].round > f.windowLength {
		f.estimates[0] = s
		f.estimates[1] = s
		f.estimates[2] = s
		return
	}

	if extraAcked > f.estimates[1].extraAcked {
		f.estimates[1] = s
		f.estimates[2] = s
	} else if extraAcked > f.estimates[2].extraAcked {
		f.estimates[2] = s
	}

	if round-f.estimates[0].round > f.windowLength {
		f.estimates[0] = f.estimates[1]
		f.estimates[1] = f.estimates[2]
		f.estimates[2] = s
		if round-f.estimates[0].round > f.windowLength {
			f.estimates[0] = f.estimates[1]
			f.estimates[1] = f.estimates[2]
		}
		return
	}

	// 1/4 window check: if second best equals best and is older than 1/4 window
	quarterWindow := f.windowLength / 4
	if f.estimates[1] != nil && f.estimates[0] != nil &&
		f.estimates[1].extraAcked == f.estimates[0].extraAcked &&
		round-f.estimates[1].round > quarterWindow {
		f.estimates[1] = s
		f.estimates[2] = s
		return
	}

	// 1/2 window check: if third best equals second best and is older than 1/2 window
	halfWindow := f.windowLength / 2
	if f.estimates[2] != nil && f.estimates[1] != nil &&
		f.estimates[2].extraAcked == f.estimates[1].extraAcked &&
		round-f.estimates[2].round > halfWindow {
		f.estimates[2] = s
	}
}

// maxAckHeightTracker tracks the maximum ack height (aggregation).
type maxAckHeightTracker struct {
	maxAckHeightFilter                     *extraAckedFilter
	aggregationEpochStartTime              monotime.Time
	aggregationEpochBytes                  congestion.ByteCount
	lastSentPacketNumberBeforeEpoch        congestion.PacketNumber
	numAckAggregationEpochs                uint64
	ackAggregationBandwidthThreshold       float64
	startNewAggregationEpochAfterFullRound bool
	reduceExtraAckedOnBandwidthIncrease    bool
}

func newMaxAckHeightTracker(window RoundTripCount, overestimateAvoidance bool) *maxAckHeightTracker {
	threshold := 1.0
	if overestimateAvoidance {
		threshold = 2.0
	}
	return &maxAckHeightTracker{
		maxAckHeightFilter:                     &extraAckedFilter{windowLength: window},
		ackAggregationBandwidthThreshold:       threshold,
		startNewAggregationEpochAfterFullRound: true,
		reduceExtraAckedOnBandwidthIncrease:    true,
	}
}

func (t *maxAckHeightTracker) update(
	bandwidthEstimate Bandwidth,
	isNewMaxBandwidth bool,
	roundTripCount RoundTripCount,
	lastSentPacketNumber congestion.PacketNumber,
	lastAckedPacketNumber congestion.PacketNumber,
	ackTime monotime.Time,
	bytesAcked congestion.ByteCount,
) congestion.ByteCount {
	forceNewEpoch := false

	// When bandwidth increases, reduce extra_acked by recalculating with new bandwidth
	if t.reduceExtraAckedOnBandwidthIncrease && isNewMaxBandwidth {
		// Save existing entries
		best := t.maxAckHeightFilter.getBest()
		secondBest := t.maxAckHeightFilter.getSecondBest()
		thirdBest := t.maxAckHeightFilter.getThirdBest()
		t.maxAckHeightFilter.clear()

		// Recalculate and reinsert with new bandwidth estimate
		if best != nil && best.timeDelta > 0 {
			expectedBytesAcked := BytesFromBandwidthAndTimeDelta(bandwidthEstimate, best.timeDelta)
			if expectedBytesAcked < best.bytesAcked {
				newExtraAcked := best.bytesAcked - expectedBytesAcked
				t.maxAckHeightFilter.update(newExtraAcked, best.bytesAcked, best.timeDelta, best.round)
			}
		}

		if secondBest != nil && secondBest != best && secondBest.timeDelta > 0 {
			expectedBytesAcked := BytesFromBandwidthAndTimeDelta(bandwidthEstimate, secondBest.timeDelta)
			if expectedBytesAcked < secondBest.bytesAcked {
				newExtraAcked := secondBest.bytesAcked - expectedBytesAcked
				t.maxAckHeightFilter.update(newExtraAcked, secondBest.bytesAcked, secondBest.timeDelta, secondBest.round)
			}
		}

		if thirdBest != nil && thirdBest != secondBest && thirdBest != best && thirdBest.timeDelta > 0 {
			expectedBytesAcked := BytesFromBandwidthAndTimeDelta(bandwidthEstimate, thirdBest.timeDelta)
			if expectedBytesAcked < thirdBest.bytesAcked {
				newExtraAcked := thirdBest.bytesAcked - expectedBytesAcked
				t.maxAckHeightFilter.update(newExtraAcked, thirdBest.bytesAcked, thirdBest.timeDelta, thirdBest.round)
			}
		}
	}

	// If any packet sent after the start of the epoch has been acked, start a new epoch.
	if t.startNewAggregationEpochAfterFullRound &&
		lastAckedPacketNumber > t.lastSentPacketNumberBeforeEpoch {
		forceNewEpoch = true
	}

	if t.aggregationEpochStartTime.IsZero() || forceNewEpoch {
		t.aggregationEpochBytes = bytesAcked
		t.aggregationEpochStartTime = ackTime
		t.lastSentPacketNumberBeforeEpoch = lastSentPacketNumber
		t.numAckAggregationEpochs++
		return 0
	}

	// Compute how many bytes are expected to be delivered.
	aggregationDelta := ackTime.Sub(t.aggregationEpochStartTime)
	expectedBytesAcked := BytesFromBandwidthAndTimeDelta(bandwidthEstimate, aggregationDelta)

	// Reset the current aggregation epoch as soon as the ack arrival rate is
	// less than or equal to the max bandwidth.
	if t.aggregationEpochBytes <= congestion.ByteCount(t.ackAggregationBandwidthThreshold*float64(expectedBytesAcked)) {
		t.aggregationEpochBytes = bytesAcked
		t.aggregationEpochStartTime = ackTime
		t.lastSentPacketNumberBeforeEpoch = lastSentPacketNumber
		t.numAckAggregationEpochs++
		return 0
	}

	t.aggregationEpochBytes += bytesAcked

	// Compute how many extra bytes were delivered vs max bandwidth.
	extraBytesAcked := t.aggregationEpochBytes - expectedBytesAcked

	t.maxAckHeightFilter.update(extraBytesAcked, t.aggregationEpochBytes, aggregationDelta, roundTripCount)
	return extraBytesAcked
}

func (t *maxAckHeightTracker) maxAckHeight() congestion.ByteCount {
	best := t.maxAckHeightFilter.getBest()
	if best == nil {
		return 0
	}
	return best.extraAcked
}

// connectionStateMap stores state for sent packets indexed by packet number.
type connectionStateMap struct {
	packets map[congestion.PacketNumber]*connectionStateOnSentPacket
}

func newConnectionStateMap() *connectionStateMap {
	return &connectionStateMap{
		packets: make(map[congestion.PacketNumber]*connectionStateOnSentPacket),
	}
}

func (m *connectionStateMap) insert(pktNum congestion.PacketNumber, state *connectionStateOnSentPacket) {
	m.packets[pktNum] = state
}

func (m *connectionStateMap) take(pktNum congestion.PacketNumber) *connectionStateOnSentPacket {
	state, ok := m.packets[pktNum]
	if !ok {
		return nil
	}
	delete(m.packets, pktNum)
	return state
}

func (m *connectionStateMap) removeObsolete(leastAcked congestion.PacketNumber) {
	for pktNum := range m.packets {
		if pktNum < leastAcked {
			delete(m.packets, pktNum)
		}
	}
}

// BandwidthSampler tracks bandwidth samples from acknowledged packets.
type BandwidthSampler struct {
	totalBytesSent     congestion.ByteCount
	totalBytesAcked    congestion.ByteCount
	totalBytesLost     congestion.ByteCount
	totalBytesNeutered congestion.ByteCount

	lastSentPacket  congestion.PacketNumber
	lastAckedPacket congestion.PacketNumber

	isAppLimited         bool
	endOfAppLimitedPhase congestion.PacketNumber

	lastAckedPacketAckTime          monotime.Time
	totalBytesSentAtLastAckedPacket congestion.ByteCount
	lastAckedPacketSentTime         monotime.Time

	connectionStateMap  *connectionStateMap
	maxAckHeightTracker *maxAckHeightTracker

	recentAckPoints recentAckPoints
	a0Candidates    []*ackPoint

	overestimateAvoidance bool
	chooseA0PointFix      bool

	totalBytesAckedAfterLastAckEvent congestion.ByteCount
}

// NewBandwidthSampler creates a new bandwidth sampler.
func NewBandwidthSampler(
	maxHeightTrackerWindowLength RoundTripCount,
	overestimateAvoidance bool,
	chooseA0PointFix bool,
) *BandwidthSampler {
	return &BandwidthSampler{
		isAppLimited:          true,
		endOfAppLimitedPhase:  invalidPacketNumber,
		connectionStateMap:    newConnectionStateMap(),
		maxAckHeightTracker:   newMaxAckHeightTracker(maxHeightTrackerWindowLength, overestimateAvoidance),
		overestimateAvoidance: overestimateAvoidance,
		chooseA0PointFix:      chooseA0PointFix,
	}
}

// OnPacketSent records a sent packet.
func (s *BandwidthSampler) OnPacketSent(
	sentTime monotime.Time,
	packetNumber congestion.PacketNumber,
	bytes congestion.ByteCount,
	bytesInFlight congestion.ByteCount,
	hasRetransmittableData bool,
) {
	s.lastSentPacket = packetNumber

	if !hasRetransmittableData {
		return
	}

	s.totalBytesSent += bytes

	// If there are no packets in flight, the time at which the new
	// transmission opens can be treated as the A_0 point.
	if bytesInFlight == 0 {
		s.lastAckedPacketAckTime = sentTime
		if s.overestimateAvoidance {
			s.recentAckPoints.clear()
			s.recentAckPoints.update(sentTime, s.totalBytesAcked)
			s.a0Candidates = s.a0Candidates[:0]
			if p := s.recentAckPoints.mostRecent(); p != nil {
				s.a0Candidates = append(s.a0Candidates, p)
			}
		}
		s.totalBytesSentAtLastAckedPacket = s.totalBytesSent
		s.lastAckedPacketSentTime = sentTime
	}

	state := &connectionStateOnSentPacket{
		sentTime:                        sentTime,
		size:                            bytes,
		totalBytesSentAtLastAckedPacket: s.totalBytesSentAtLastAckedPacket,
		lastAckedPacketSentTime:         s.lastAckedPacketSentTime,
		lastAckedPacketAckTime:          s.lastAckedPacketAckTime,
		sendTimeState: SendTimeState{
			IsValid:         true,
			IsAppLimited:    s.isAppLimited,
			TotalBytesSent:  s.totalBytesSent,
			TotalBytesAcked: s.totalBytesAcked,
			TotalBytesLost:  s.totalBytesLost,
			BytesInFlight:   bytesInFlight + bytes,
		},
	}
	s.connectionStateMap.insert(packetNumber, state)
}

// OnPacketNeutered handles a neutered (cancelled) packet.
func (s *BandwidthSampler) OnPacketNeutered(packetNumber congestion.PacketNumber) {
	state := s.connectionStateMap.take(packetNumber)
	if state != nil {
		s.totalBytesNeutered += state.size
	}
}

// OnCongestionEvent processes acked and lost packets.
func (s *BandwidthSampler) OnCongestionEvent(
	ackTime monotime.Time,
	ackedPackets []congestion.AckedPacketInfo,
	lostPackets []congestion.LostPacketInfo,
	maxBandwidth Bandwidth,
	estBandwidthUpperBound Bandwidth,
	roundTripCount RoundTripCount,
) CongestionEventSample {
	var lastLostPacketSendState SendTimeState
	var lastAckedPacketSendState SendTimeState
	var lastLostPacketNum congestion.PacketNumber
	var lastAckedPacketNum congestion.PacketNumber

	// Process lost packets
	for _, packet := range lostPackets {
		sendState := s.onPacketLost(packet.PacketNumber, packet.BytesLost)
		if sendState.IsValid {
			lastLostPacketSendState = sendState
			lastLostPacketNum = packet.PacketNumber
		}
	}

	if len(ackedPackets) == 0 {
		return CongestionEventSample{
			LastPacketSendState: lastLostPacketSendState,
		}
	}

	var eventSample CongestionEventSample
	var isNewMaxBandwidth bool

	// Process acked packets
	for _, packet := range ackedPackets {
		sample := s.onPacketAcknowledged(ackTime, packet.PacketNumber, packet.SentTime)
		if sample == nil || !sample.stateAtSend.IsValid {
			continue
		}

		lastAckedPacketSendState = sample.stateAtSend
		lastAckedPacketNum = packet.PacketNumber

		if eventSample.SampleRtt == 0 || sample.rtt < eventSample.SampleRtt {
			eventSample.SampleRtt = sample.rtt
		}

		if sample.bandwidth > eventSample.SampleMaxBandwidth {
			eventSample.SampleMaxBandwidth = sample.bandwidth
			eventSample.SampleIsAppLimited = sample.stateAtSend.IsAppLimited
		}

		inflightSample := s.totalBytesAcked - lastAckedPacketSendState.TotalBytesAcked
		if inflightSample > eventSample.SampleMaxInflight {
			eventSample.SampleMaxInflight = inflightSample
		}
	}

	// Determine last packet send state
	if !lastLostPacketSendState.IsValid {
		eventSample.LastPacketSendState = lastAckedPacketSendState
	} else if !lastAckedPacketSendState.IsValid {
		eventSample.LastPacketSendState = lastLostPacketSendState
	} else if lastAckedPacketNum > lastLostPacketNum {
		eventSample.LastPacketSendState = lastAckedPacketSendState
	} else {
		eventSample.LastPacketSendState = lastLostPacketSendState
	}

	isNewMaxBandwidth = eventSample.SampleMaxBandwidth > maxBandwidth
	if eventSample.SampleMaxBandwidth > maxBandwidth {
		maxBandwidth = eventSample.SampleMaxBandwidth
	}

	bandwidthEstimate := maxBandwidth
	if bandwidthEstimate > estBandwidthUpperBound {
		bandwidthEstimate = estBandwidthUpperBound
	}
	if bandwidthEstimate == 0 {
		bandwidthEstimate = estBandwidthUpperBound
	}

	eventSample.ExtraAcked = s.onAckEventEnd(bandwidthEstimate, isNewMaxBandwidth, roundTripCount)

	return eventSample
}

// bandwidthSample is an internal sample from a single acknowledged packet.
type bandwidthSample struct {
	bandwidth   Bandwidth
	rtt         time.Duration
	sendRate    Bandwidth
	stateAtSend SendTimeState
}

func (s *BandwidthSampler) onPacketLost(packetNumber congestion.PacketNumber, bytesLost congestion.ByteCount) SendTimeState {
	var sendTimeState SendTimeState

	s.totalBytesLost += bytesLost
	state := s.connectionStateMap.take(packetNumber)
	if state != nil {
		sendTimeState = state.sendTimeState
		sendTimeState.IsValid = true
	}

	return sendTimeState
}

func (s *BandwidthSampler) onPacketAcknowledged(
	ackTime monotime.Time,
	packetNumber congestion.PacketNumber,
	sentTime monotime.Time,
) *bandwidthSample {
	s.lastAckedPacket = packetNumber
	sentPacket := s.connectionStateMap.take(packetNumber)
	if sentPacket == nil {
		return nil
	}

	s.totalBytesAcked += sentPacket.size
	s.totalBytesSentAtLastAckedPacket = sentPacket.sendTimeState.TotalBytesSent
	s.lastAckedPacketSentTime = sentPacket.sentTime
	s.lastAckedPacketAckTime = ackTime

	if s.overestimateAvoidance {
		s.recentAckPoints.update(ackTime, s.totalBytesAcked)
	}

	// Check if we should exit app-limited phase
	if s.isAppLimited {
		if s.endOfAppLimitedPhase == invalidPacketNumber ||
			packetNumber > s.endOfAppLimitedPhase {
			s.isAppLimited = false
		}
	}

	// Calculate send rate
	var sendRate Bandwidth
	if sentPacket.sentTime.After(sentPacket.lastAckedPacketSentTime) {
		bytesSent := sentPacket.sendTimeState.TotalBytesSent - sentPacket.totalBytesSentAtLastAckedPacket
		timeDelta := sentPacket.sentTime.Sub(sentPacket.lastAckedPacketSentTime)
		sendRate = BandwidthFromBytesAndTimeDelta(bytesSent, timeDelta)
	}

	// Choose A0 point for bandwidth calculation
	var a0 *ackPoint
	if s.overestimateAvoidance {
		a0 = s.chooseA0Point(sentPacket.sendTimeState.TotalBytesAcked)
	}

	if a0 == nil {
		a0 = &ackPoint{
			ackTime:         sentPacket.lastAckedPacketAckTime,
			totalBytesAcked: sentPacket.sendTimeState.TotalBytesAcked,
		}
	}

	// Ensure ack time is strictly greater than A0 ack time
	if !ackTime.After(a0.ackTime) {
		return nil
	}

	// Calculate ack rate
	ackTimeDelta := ackTime.Sub(a0.ackTime)
	bytesAcked := s.totalBytesAcked - a0.totalBytesAcked
	ackRate := BandwidthFromBytesAndTimeDelta(bytesAcked, ackTimeDelta)

	// Bandwidth is min of send rate and ack rate
	bandwidth := ackRate
	if sendRate > 0 && sendRate < ackRate {
		bandwidth = sendRate
	}

	// Calculate RTT (does not account for delayed ack)
	rtt := ackTime.Sub(sentPacket.sentTime)

	return &bandwidthSample{
		bandwidth:   bandwidth,
		rtt:         rtt,
		sendRate:    sendRate,
		stateAtSend: sentPacket.sendTimeState,
	}
}

func (s *BandwidthSampler) chooseA0Point(totalBytesAcked congestion.ByteCount) *ackPoint {
	if len(s.a0Candidates) == 0 {
		return nil
	}

	// Remove candidates that are too old
	for len(s.a0Candidates) > 1 {
		if s.a0Candidates[1].totalBytesAcked > totalBytesAcked {
			if s.chooseA0PointFix {
				break
			}
			return s.a0Candidates[1]
		}
		s.a0Candidates = s.a0Candidates[1:]
	}

	return s.a0Candidates[0]
}

func (s *BandwidthSampler) onAckEventEnd(
	bandwidthEstimate Bandwidth,
	isNewMaxBandwidth bool,
	roundTripCount RoundTripCount,
) congestion.ByteCount {
	newlyAckedBytes := s.totalBytesAcked - s.totalBytesAckedAfterLastAckEvent
	if newlyAckedBytes == 0 {
		return 0
	}

	s.totalBytesAckedAfterLastAckEvent = s.totalBytesAcked
	extraAcked := s.maxAckHeightTracker.update(
		bandwidthEstimate,
		isNewMaxBandwidth,
		roundTripCount,
		s.lastSentPacket,
		s.lastAckedPacket,
		s.lastAckedPacketAckTime,
		newlyAckedBytes,
	)

	// If extraAcked is zero, save lessRecentPoint as an A0 candidate
	if s.overestimateAvoidance && extraAcked == 0 {
		if p := s.recentAckPoints.lessRecentPoint(s.chooseA0PointFix); p != nil {
			s.a0Candidates = append(s.a0Candidates, p)
		}
	}

	return extraAcked
}

// TotalBytesAcked returns the total bytes acked.
func (s *BandwidthSampler) TotalBytesAcked() congestion.ByteCount {
	return s.totalBytesAcked
}

// TotalBytesLost returns the total bytes lost.
func (s *BandwidthSampler) TotalBytesLost() congestion.ByteCount {
	return s.totalBytesLost
}

// MaxAckHeight returns the maximum ack height.
func (s *BandwidthSampler) MaxAckHeight() congestion.ByteCount {
	return s.maxAckHeightTracker.maxAckHeight()
}

// OnAppLimited marks the connection as app limited.
func (s *BandwidthSampler) OnAppLimited() {
	s.isAppLimited = true
	s.endOfAppLimitedPhase = s.lastSentPacket
}

// IsAppLimited returns whether the connection is app limited.
func (s *BandwidthSampler) IsAppLimited() bool {
	return s.isAppLimited
}

// RemoveObsoletePackets removes packets that are no longer needed.
func (s *BandwidthSampler) RemoveObsoletePackets(leastAcked congestion.PacketNumber) {
	s.connectionStateMap.removeObsolete(leastAcked)
}
