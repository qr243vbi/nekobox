// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ported from:
// https://github.com/google/quiche/blob/main/quiche/quic/core/congestion_control/bandwidth_sampler.h
// https://github.com/google/quiche/blob/main/quiche/quic/core/congestion_control/bandwidth_sampler.cc

package congestion_bbr1

import (
	"time"

	"github.com/sagernet/quic-go/congestion"
	"github.com/sagernet/quic-go/monotime"
)

// SendTimeState is a subset of ConnectionStateOnSentPacket which is returned
// to the caller when the packet is acked or lost.
type SendTimeState struct {
	// Whether other states in this object is valid.
	IsValid bool
	// Whether the sender is app limited at the time the packet was sent.
	IsAppLimited bool
	// Total number of sent bytes at the time the packet was sent.
	TotalBytesSent congestion.ByteCount
	// Total number of acked bytes at the time the packet was sent.
	TotalBytesAcked congestion.ByteCount
	// Total number of lost bytes at the time the packet was sent.
	TotalBytesLost congestion.ByteCount
	// Total number of inflight bytes at the time the packet was sent.
	BytesInFlight congestion.ByteCount
}

// ExtraAckedEvent represents the excess bytes acknowledged in a time delta.
type ExtraAckedEvent struct {
	// The excess bytes acknowledged in the time delta for this event.
	ExtraAcked congestion.ByteCount
	// The bytes acknowledged and time delta from the event.
	BytesAcked congestion.ByteCount
	TimeDelta  time.Duration
	// The round trip of the event.
	Round uint64
}

// BandwidthSample represents a bandwidth measurement from a single packet.
type BandwidthSample struct {
	// The bandwidth at that particular sample. Zero if no valid bandwidth sample
	// is available.
	Bandwidth Bandwidth
	// The RTT measurement at this particular sample. Zero if no RTT sample is
	// available. Does not correct for delayed ack time.
	RTT time.Duration
	// send_rate is computed from the current packet being acked('P') and an
	// earlier packet that is acked before P was sent.
	SendRate Bandwidth
	// States captured when the packet was sent.
	StateAtSend SendTimeState
}

// CongestionEventSample contains aggregated information from a congestion event.
type CongestionEventSample struct {
	// The maximum bandwidth sample from all acked packets.
	SampleMaxBandwidth Bandwidth
	// Whether SampleMaxBandwidth is from a app-limited sample.
	SampleIsAppLimited bool
	// The minimum rtt sample from all acked packets.
	SampleRTT time.Duration
	// For each packet p in acked packets, this is the max value of INFLIGHT(p).
	SampleMaxInflight congestion.ByteCount
	// The send state of the largest packet in acked_packets, unless it is empty.
	LastPacketSendState SendTimeState
	// The number of extra bytes acked from this ack event.
	ExtraAcked congestion.ByteCount
}

// AckPoint represents a point on the ack line.
type AckPoint struct {
	AckTime         monotime.Time
	TotalBytesAcked congestion.ByteCount
}

// RecentAckPoints maintains the most recent 2 ack points at distinct times.
type RecentAckPoints struct {
	ackPoints [2]AckPoint
}

// Update updates the ack points with a new ack.
func (r *RecentAckPoints) Update(ackTime monotime.Time, totalBytesAcked congestion.ByteCount) {
	if ackTime.Before(r.ackPoints[1].AckTime) {
		// This can only happen when time goes backwards.
		r.ackPoints[1].AckTime = ackTime
	} else if ackTime.After(r.ackPoints[1].AckTime) {
		r.ackPoints[0] = r.ackPoints[1]
		r.ackPoints[1].AckTime = ackTime
	}
	r.ackPoints[1].TotalBytesAcked = totalBytesAcked
}

// Clear clears the ack points.
func (r *RecentAckPoints) Clear() {
	r.ackPoints[0] = AckPoint{}
	r.ackPoints[1] = AckPoint{}
}

// MostRecentPoint returns the most recent ack point.
func (r *RecentAckPoints) MostRecentPoint() AckPoint {
	return r.ackPoints[1]
}

// LessRecentPoint returns the less recent ack point.
func (r *RecentAckPoints) LessRecentPoint() AckPoint {
	if r.ackPoints[0].TotalBytesAcked != 0 {
		return r.ackPoints[0]
	}
	return r.ackPoints[1]
}

// ConnectionStateOnSentPacket represents the information about a sent packet
// and the state of the connection at the moment the packet was sent.
type ConnectionStateOnSentPacket struct {
	// Time at which the packet is sent.
	SentTime monotime.Time
	// Size of the packet.
	Size congestion.ByteCount
	// The value of totalBytesSentAtLastAckedPacket at the time the packet was sent.
	TotalBytesSentAtLastAckedPacket congestion.ByteCount
	// The value of lastAckedPacketSentTime at the time the packet was sent.
	LastAckedPacketSentTime monotime.Time
	// The value of lastAckedPacketAckTime at the time the packet was sent.
	LastAckedPacketAckTime monotime.Time
	// Send time states that are returned to the congestion controller when the
	// packet is acked or lost.
	SendTimeState SendTimeState
}

// extraAckedEventFilter is a specialized windowed filter for ExtraAckedEvent.
type extraAckedEventFilter struct {
	windowLength uint64
	estimates    [3]struct {
		value ExtraAckedEvent
		time  uint64
	}
}

func newExtraAckedEventFilter(windowLength uint64) *extraAckedEventFilter {
	return &extraAckedEventFilter{windowLength: windowLength}
}

func (f *extraAckedEventFilter) SetWindowLength(length uint64) {
	f.windowLength = length
}

func (f *extraAckedEventFilter) Update(newSample ExtraAckedEvent, newTime uint64) {
	// Reset all estimates if they have not yet been initialized, if new sample
	// is a new best, or if the newest recorded estimate is too old.
	if f.estimates[0].value.ExtraAcked == 0 ||
		newSample.ExtraAcked >= f.estimates[0].value.ExtraAcked ||
		newTime-f.estimates[2].time > f.windowLength {
		f.Reset(newSample, newTime)
		return
	}

	if newSample.ExtraAcked >= f.estimates[1].value.ExtraAcked {
		f.estimates[1].value = newSample
		f.estimates[1].time = newTime
		f.estimates[2] = f.estimates[1]
	} else if newSample.ExtraAcked >= f.estimates[2].value.ExtraAcked {
		f.estimates[2].value = newSample
		f.estimates[2].time = newTime
	}

	// Expire and update estimates as necessary.
	if newTime-f.estimates[0].time > f.windowLength {
		f.estimates[0] = f.estimates[1]
		f.estimates[1] = f.estimates[2]
		f.estimates[2].value = newSample
		f.estimates[2].time = newTime
		if newTime-f.estimates[0].time > f.windowLength {
			f.estimates[0] = f.estimates[1]
			f.estimates[1] = f.estimates[2]
		}
		return
	}

	if f.estimates[1].value.ExtraAcked == f.estimates[0].value.ExtraAcked &&
		newTime-f.estimates[1].time > f.windowLength>>2 {
		f.estimates[1].value = newSample
		f.estimates[1].time = newTime
		f.estimates[2] = f.estimates[1]
		return
	}

	if f.estimates[2].value.ExtraAcked == f.estimates[1].value.ExtraAcked &&
		newTime-f.estimates[2].time > f.windowLength>>1 {
		f.estimates[2].value = newSample
		f.estimates[2].time = newTime
	}
}

func (f *extraAckedEventFilter) Reset(newSample ExtraAckedEvent, newTime uint64) {
	f.estimates[0].value = newSample
	f.estimates[0].time = newTime
	f.estimates[1] = f.estimates[0]
	f.estimates[2] = f.estimates[0]
}

func (f *extraAckedEventFilter) Clear() {
	f.Reset(ExtraAckedEvent{}, 0)
}

func (f *extraAckedEventFilter) GetBest() ExtraAckedEvent {
	return f.estimates[0].value
}

func (f *extraAckedEventFilter) GetSecondBest() ExtraAckedEvent {
	return f.estimates[1].value
}

func (f *extraAckedEventFilter) GetThirdBest() ExtraAckedEvent {
	return f.estimates[2].value
}

// MaxAckHeightTracker keeps track of the degree of ack aggregation.
type MaxAckHeightTracker struct {
	// Tracks the maximum number of bytes acked faster than the estimated bandwidth.
	maxAckHeightFilter *extraAckedEventFilter
	// The time this aggregation started and the number of bytes acked during it.
	aggregationEpochStartTime monotime.Time
	aggregationEpochBytes     congestion.ByteCount
	// The last sent packet number before the current aggregation epoch started.
	lastSentPacketNumberBeforeEpoch congestion.PacketNumber
	// The number of ack aggregation epochs ever started, including the ongoing one.
	numAckAggregationEpochs uint64
	// Threshold for starting a new aggregation epoch.
	ackAggregationBandwidthThreshold float64
	// Configuration options
	startNewAggregationEpochAfterFullRound bool
	reduceExtraAckedOnBandwidthIncrease    bool
}

// NewMaxAckHeightTracker creates a new MaxAckHeightTracker.
func NewMaxAckHeightTracker(initialFilterWindow uint64) *MaxAckHeightTracker {
	return &MaxAckHeightTracker{
		maxAckHeightFilter:               newExtraAckedEventFilter(initialFilterWindow),
		ackAggregationBandwidthThreshold: 1.0, // Default threshold
	}
}

// Get returns the current max ack height.
func (t *MaxAckHeightTracker) Get() congestion.ByteCount {
	return t.maxAckHeightFilter.GetBest().ExtraAcked
}

// Update updates the tracker with new ack information.
func (t *MaxAckHeightTracker) Update(
	bandwidthEstimate Bandwidth,
	isNewMaxBandwidth bool,
	roundTripCount uint64,
	lastSentPacketNumber congestion.PacketNumber,
	lastAckedPacketNumber congestion.PacketNumber,
	ackTime monotime.Time,
	bytesAcked congestion.ByteCount,
) congestion.ByteCount {
	forceNewEpoch := false

	if t.reduceExtraAckedOnBandwidthIncrease && isNewMaxBandwidth {
		// Save and clear existing entries, then reinsert with recalculated values.
		best := t.maxAckHeightFilter.GetBest()
		secondBest := t.maxAckHeightFilter.GetSecondBest()
		thirdBest := t.maxAckHeightFilter.GetThirdBest()
		t.maxAckHeightFilter.Clear()

		// Reinsert with recalculated extra_acked
		expectedBytesAcked := bandwidthEstimate.ToBytesPerPeriod(best.TimeDelta)
		if expectedBytesAcked < best.BytesAcked {
			best.ExtraAcked = best.BytesAcked - expectedBytesAcked
			t.maxAckHeightFilter.Update(best, best.Round)
		}
		expectedBytesAcked = bandwidthEstimate.ToBytesPerPeriod(secondBest.TimeDelta)
		if expectedBytesAcked < secondBest.BytesAcked {
			secondBest.ExtraAcked = secondBest.BytesAcked - expectedBytesAcked
			t.maxAckHeightFilter.Update(secondBest, secondBest.Round)
		}
		expectedBytesAcked = bandwidthEstimate.ToBytesPerPeriod(thirdBest.TimeDelta)
		if expectedBytesAcked < thirdBest.BytesAcked {
			thirdBest.ExtraAcked = thirdBest.BytesAcked - expectedBytesAcked
			t.maxAckHeightFilter.Update(thirdBest, thirdBest.Round)
		}
	}

	// If any packet sent after the start of the epoch has been acked, start a new epoch.
	if t.startNewAggregationEpochAfterFullRound &&
		t.lastSentPacketNumberBeforeEpoch != 0 &&
		lastAckedPacketNumber != 0 &&
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

	// Compute how many bytes are expected to be delivered, assuming max bandwidth is correct.
	aggregationDelta := ackTime.Sub(t.aggregationEpochStartTime)
	expectedBytesAcked := bandwidthEstimate.ToBytesPerPeriod(aggregationDelta)

	// Reset the current aggregation epoch as soon as the ack arrival rate is less
	// than or equal to the max bandwidth.
	if float64(t.aggregationEpochBytes) <= t.ackAggregationBandwidthThreshold*float64(expectedBytesAcked) {
		// Reset to start measuring a new aggregation epoch.
		t.aggregationEpochBytes = bytesAcked
		t.aggregationEpochStartTime = ackTime
		t.lastSentPacketNumberBeforeEpoch = lastSentPacketNumber
		t.numAckAggregationEpochs++
		return 0
	}

	t.aggregationEpochBytes += bytesAcked

	// Compute how many extra bytes were delivered vs max bandwidth.
	extraBytesAcked := t.aggregationEpochBytes - expectedBytesAcked

	newEvent := ExtraAckedEvent{
		ExtraAcked: extraBytesAcked,
		BytesAcked: t.aggregationEpochBytes,
		TimeDelta:  aggregationDelta,
		Round:      roundTripCount,
	}
	t.maxAckHeightFilter.Update(newEvent, roundTripCount)
	return extraBytesAcked
}

// SetFilterWindowLength sets the window length for the filter.
func (t *MaxAckHeightTracker) SetFilterWindowLength(length uint64) {
	t.maxAckHeightFilter.SetWindowLength(length)
}

// Reset resets the tracker.
func (t *MaxAckHeightTracker) Reset(newHeight congestion.ByteCount, newTime uint64) {
	newEvent := ExtraAckedEvent{
		ExtraAcked: newHeight,
		Round:      newTime,
	}
	t.maxAckHeightFilter.Reset(newEvent, newTime)
}

// SetAckAggregationBandwidthThreshold sets the threshold.
func (t *MaxAckHeightTracker) SetAckAggregationBandwidthThreshold(threshold float64) {
	t.ackAggregationBandwidthThreshold = threshold
}

// SetStartNewAggregationEpochAfterFullRound sets the option.
func (t *MaxAckHeightTracker) SetStartNewAggregationEpochAfterFullRound(value bool) {
	t.startNewAggregationEpochAfterFullRound = value
}

// SetReduceExtraAckedOnBandwidthIncrease sets the option.
func (t *MaxAckHeightTracker) SetReduceExtraAckedOnBandwidthIncrease(value bool) {
	t.reduceExtraAckedOnBandwidthIncrease = value
}

// NumAckAggregationEpochs returns the number of aggregation epochs.
func (t *MaxAckHeightTracker) NumAckAggregationEpochs() uint64 {
	return t.numAckAggregationEpochs
}

// BandwidthSampler keeps track of sent and acknowledged packets and outputs a
// bandwidth sample for every packet acknowledged.
type BandwidthSampler struct {
	// Total number of bytes sent/acked/lost/neutered during the connection.
	totalBytesSent     congestion.ByteCount
	totalBytesAcked    congestion.ByteCount
	totalBytesLost     congestion.ByteCount
	totalBytesNeutered congestion.ByteCount

	// The value of totalBytesSent at the time the last acknowledged packet was sent.
	totalBytesSentAtLastAckedPacket congestion.ByteCount

	// The time at which the last acknowledged packet was sent.
	lastAckedPacketSentTime monotime.Time

	// The time at which the most recent packet was acknowledged.
	lastAckedPacketAckTime monotime.Time

	// The most recently sent packet.
	lastSentPacket congestion.PacketNumber

	// The most recently acked packet.
	lastAckedPacket congestion.PacketNumber

	// Indicates whether the bandwidth sampler is currently in an app-limited phase.
	isAppLimited bool

	// The packet that will be acknowledged after this one will cause the sampler
	// to exit the app-limited phase.
	endOfAppLimitedPhase congestion.PacketNumber

	// Record of the connection state at the point where each packet in flight was
	// sent, indexed by the packet number.
	connectionStateMap *PacketNumberIndexedQueue[ConnectionStateOnSentPacket]

	// Recent ack points for overestimate avoidance.
	recentAckPoints RecentAckPoints
	a0Candidates    []AckPoint

	// Max ack height tracker.
	maxAckHeightTracker *MaxAckHeightTracker

	// Total bytes acked after the last ack event.
	totalBytesAckedAfterLastAckEvent congestion.ByteCount

	// Configuration options
	overestimateAvoidance              bool
	limitMaxAckHeightTrackerBySendRate bool
}

// NewBandwidthSampler creates a new BandwidthSampler.
func NewBandwidthSampler(maxHeightTrackerWindowLength uint64) *BandwidthSampler {
	return &BandwidthSampler{
		isAppLimited:        true, // Start in app-limited state
		connectionStateMap:  NewPacketNumberIndexedQueue[ConnectionStateOnSentPacket](),
		maxAckHeightTracker: NewMaxAckHeightTracker(maxHeightTrackerWindowLength),
	}
}

// OnPacketSent inputs the sent packet information into the sampler.
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

	// If there are no packets in flight, the time at which the new transmission
	// opens can be treated as the A_0 point for the purpose of bandwidth sampling.
	if bytesInFlight == 0 {
		s.lastAckedPacketAckTime = sentTime
		if s.overestimateAvoidance {
			s.recentAckPoints.Clear()
			s.recentAckPoints.Update(sentTime, s.totalBytesAcked)
			s.a0Candidates = s.a0Candidates[:0]
			s.a0Candidates = append(s.a0Candidates, s.recentAckPoints.MostRecentPoint())
		}
		s.totalBytesSentAtLastAckedPacket = s.totalBytesSent
		s.lastAckedPacketSentTime = sentTime
	}

	s.connectionStateMap.Emplace(packetNumber, ConnectionStateOnSentPacket{
		SentTime:                        sentTime,
		Size:                            bytes,
		TotalBytesSentAtLastAckedPacket: s.totalBytesSentAtLastAckedPacket,
		LastAckedPacketSentTime:         s.lastAckedPacketSentTime,
		LastAckedPacketAckTime:          s.lastAckedPacketAckTime,
		SendTimeState: SendTimeState{
			IsValid:         true,
			IsAppLimited:    s.isAppLimited,
			TotalBytesSent:  s.totalBytesSent,
			TotalBytesAcked: s.totalBytesAcked,
			TotalBytesLost:  s.totalBytesLost,
			BytesInFlight:   bytesInFlight + bytes,
		},
	})
}

// OnPacketNeutered handles a neutered packet.
func (s *BandwidthSampler) OnPacketNeutered(packetNumber congestion.PacketNumber) {
	s.connectionStateMap.RemoveWithCallback(packetNumber, func(sentPacket *ConnectionStateOnSentPacket) {
		s.totalBytesNeutered += sentPacket.Size
	})
}

// OnCongestionEvent processes a congestion event and returns a sample.
func (s *BandwidthSampler) OnCongestionEvent(
	ackTime monotime.Time,
	ackedPackets []congestion.AckedPacketInfo,
	lostPackets []congestion.LostPacketInfo,
	maxBandwidth Bandwidth,
	estBandwidthUpperBound Bandwidth,
	roundTripCount uint64,
) CongestionEventSample {
	eventSample := CongestionEventSample{
		SampleRTT: time.Duration(1<<63 - 1), // MaxDuration
	}

	var lastLostPacketSendState SendTimeState

	for _, packet := range lostPackets {
		sendState := s.onPacketLost(packet.PacketNumber, packet.BytesLost)
		if sendState.IsValid {
			lastLostPacketSendState = sendState
		}
	}

	if len(ackedPackets) == 0 {
		// Only populate send state for a loss-only event.
		eventSample.LastPacketSendState = lastLostPacketSendState
		return eventSample
	}

	var lastAckedPacketSendState SendTimeState
	var maxSendRate Bandwidth

	for _, packet := range ackedPackets {
		sample := s.onPacketAcknowledged(ackTime, packet.PacketNumber)
		if !sample.StateAtSend.IsValid {
			continue
		}

		lastAckedPacketSendState = sample.StateAtSend

		if sample.RTT > 0 && sample.RTT < eventSample.SampleRTT {
			eventSample.SampleRTT = sample.RTT
		}
		if sample.Bandwidth > eventSample.SampleMaxBandwidth {
			eventSample.SampleMaxBandwidth = sample.Bandwidth
			eventSample.SampleIsAppLimited = sample.StateAtSend.IsAppLimited
		}
		if !sample.SendRate.IsInfinite() {
			if sample.SendRate > maxSendRate {
				maxSendRate = sample.SendRate
			}
		}
		inflightSample := s.totalBytesAcked - lastAckedPacketSendState.TotalBytesAcked
		if inflightSample > eventSample.SampleMaxInflight {
			eventSample.SampleMaxInflight = inflightSample
		}
	}

	if !lastLostPacketSendState.IsValid {
		eventSample.LastPacketSendState = lastAckedPacketSendState
	} else if !lastAckedPacketSendState.IsValid {
		eventSample.LastPacketSendState = lastLostPacketSendState
	} else {
		// Use the send state from the packet with the larger packet number.
		if len(lostPackets) > 0 && len(ackedPackets) > 0 &&
			lostPackets[len(lostPackets)-1].PacketNumber > ackedPackets[len(ackedPackets)-1].PacketNumber {
			eventSample.LastPacketSendState = lastLostPacketSendState
		} else {
			eventSample.LastPacketSendState = lastAckedPacketSendState
		}
	}

	isNewMaxBandwidth := eventSample.SampleMaxBandwidth > maxBandwidth
	maxBandwidth = max(maxBandwidth, eventSample.SampleMaxBandwidth)
	if s.limitMaxAckHeightTrackerBySendRate {
		maxBandwidth = max(maxBandwidth, maxSendRate)
	}

	eventSample.ExtraAcked = s.onAckEventEnd(
		min(estBandwidthUpperBound, maxBandwidth),
		isNewMaxBandwidth,
		roundTripCount,
	)

	return eventSample
}

// onAckEventEnd handles the end of an ack event.
func (s *BandwidthSampler) onAckEventEnd(
	bandwidthEstimate Bandwidth,
	isNewMaxBandwidth bool,
	roundTripCount uint64,
) congestion.ByteCount {
	newlyAckedBytes := s.totalBytesAcked - s.totalBytesAckedAfterLastAckEvent
	if newlyAckedBytes == 0 {
		return 0
	}
	s.totalBytesAckedAfterLastAckEvent = s.totalBytesAcked

	extraAcked := s.maxAckHeightTracker.Update(
		bandwidthEstimate,
		isNewMaxBandwidth,
		roundTripCount,
		s.lastSentPacket,
		s.lastAckedPacket,
		s.lastAckedPacketAckTime,
		newlyAckedBytes,
	)

	// If extraAcked is zero, save LessRecentPoint as an A0 candidate.
	if s.overestimateAvoidance && extraAcked == 0 {
		s.a0Candidates = append(s.a0Candidates, s.recentAckPoints.LessRecentPoint())
	}
	return extraAcked
}

// onPacketAcknowledged handles an acknowledged packet.
func (s *BandwidthSampler) onPacketAcknowledged(
	ackTime monotime.Time,
	packetNumber congestion.PacketNumber,
) BandwidthSample {
	s.lastAckedPacket = packetNumber
	sentPacket := s.connectionStateMap.GetEntry(packetNumber)
	if sentPacket == nil {
		return BandwidthSample{}
	}
	sample := s.onPacketAcknowledgedInner(ackTime, packetNumber, *sentPacket)
	return sample
}

// onPacketAcknowledgedInner is the inner implementation of packet acknowledgment.
func (s *BandwidthSampler) onPacketAcknowledgedInner(
	ackTime monotime.Time,
	packetNumber congestion.PacketNumber,
	sentPacket ConnectionStateOnSentPacket,
) BandwidthSample {
	s.totalBytesAcked += sentPacket.Size
	s.totalBytesSentAtLastAckedPacket = sentPacket.SendTimeState.TotalBytesSent
	s.lastAckedPacketSentTime = sentPacket.SentTime
	s.lastAckedPacketAckTime = ackTime

	if s.overestimateAvoidance {
		s.recentAckPoints.Update(ackTime, s.totalBytesAcked)
	}

	if s.isAppLimited {
		// Exit app-limited phase if appropriate.
		if s.endOfAppLimitedPhase == 0 || packetNumber > s.endOfAppLimitedPhase {
			s.isAppLimited = false
		}
	}

	// There might have been no packets acknowledged at the moment when the
	// current packet was sent. In that case, there is no bandwidth sample to make.
	if sentPacket.LastAckedPacketSentTime.IsZero() {
		return BandwidthSample{}
	}

	// Infinite rate indicates that the sampler is supposed to discard the
	// current send rate sample and use only the ack rate.
	sendRate := InfiniteBandwidth()
	if sentPacket.SentTime.After(sentPacket.LastAckedPacketSentTime) {
		sendRate = BandwidthFromBytesAndTimeDelta(
			sentPacket.SendTimeState.TotalBytesSent-sentPacket.TotalBytesSentAtLastAckedPacket,
			sentPacket.SentTime.Sub(sentPacket.LastAckedPacketSentTime),
		)
	}

	var a0 AckPoint
	if s.overestimateAvoidance && s.chooseA0Point(sentPacket.SendTimeState.TotalBytesAcked, &a0) {
		// Use the chosen a0 point.
	} else {
		a0.AckTime = sentPacket.LastAckedPacketAckTime
		a0.TotalBytesAcked = sentPacket.SendTimeState.TotalBytesAcked
	}

	// During the slope calculation, ensure that ack time of the current packet is
	// always larger than the time of the previous packet.
	if !ackTime.After(a0.AckTime) {
		return BandwidthSample{}
	}

	ackRate := BandwidthFromBytesAndTimeDelta(
		s.totalBytesAcked-a0.TotalBytesAcked,
		ackTime.Sub(a0.AckTime),
	)

	sample := BandwidthSample{
		Bandwidth: min(sendRate, ackRate),
		RTT:       ackTime.Sub(sentPacket.SentTime),
		SendRate:  sendRate,
	}
	s.sentPacketToSendTimeState(sentPacket, &sample.StateAtSend)

	return sample
}

// chooseA0Point chooses the best a0 from a0Candidates to calculate the ack rate.
func (s *BandwidthSampler) chooseA0Point(totalBytesAcked congestion.ByteCount, a0 *AckPoint) bool {
	if len(s.a0Candidates) == 0 {
		return false
	}

	if len(s.a0Candidates) == 1 {
		*a0 = s.a0Candidates[0]
		return true
	}

	for i := 1; i < len(s.a0Candidates); i++ {
		if s.a0Candidates[i].TotalBytesAcked > totalBytesAcked {
			*a0 = s.a0Candidates[i-1]
			if i > 1 {
				s.a0Candidates = s.a0Candidates[i-1:]
			}
			return true
		}
	}

	// All candidates' total_bytes_acked is <= totalBytesAcked.
	*a0 = s.a0Candidates[len(s.a0Candidates)-1]
	s.a0Candidates = s.a0Candidates[len(s.a0Candidates)-1:]
	return true
}

// onPacketLost handles a lost packet.
func (s *BandwidthSampler) onPacketLost(
	packetNumber congestion.PacketNumber,
	bytesLost congestion.ByteCount,
) SendTimeState {
	s.totalBytesLost += bytesLost

	var sendTimeState SendTimeState
	sentPacket := s.connectionStateMap.GetEntry(packetNumber)
	if sentPacket != nil {
		s.sentPacketToSendTimeState(*sentPacket, &sendTimeState)
	}
	return sendTimeState
}

// sentPacketToSendTimeState copies the send time state from a sent packet.
func (s *BandwidthSampler) sentPacketToSendTimeState(
	sentPacket ConnectionStateOnSentPacket,
	sendTimeState *SendTimeState,
) {
	*sendTimeState = sentPacket.SendTimeState
	sendTimeState.IsValid = true
}

// OnAppLimited informs the sampler that the connection is currently app-limited.
func (s *BandwidthSampler) OnAppLimited() {
	s.isAppLimited = true
	s.endOfAppLimitedPhase = s.lastSentPacket
}

// RemoveObsoletePackets removes all the packets lower than the specified packet number.
func (s *BandwidthSampler) RemoveObsoletePackets(leastUnacked congestion.PacketNumber) {
	s.connectionStateMap.RemoveUpTo(leastUnacked)
}

// TotalBytesSent returns the total bytes sent.
func (s *BandwidthSampler) TotalBytesSent() congestion.ByteCount {
	return s.totalBytesSent
}

// TotalBytesAcked returns the total bytes acked.
func (s *BandwidthSampler) TotalBytesAcked() congestion.ByteCount {
	return s.totalBytesAcked
}

// TotalBytesLost returns the total bytes lost.
func (s *BandwidthSampler) TotalBytesLost() congestion.ByteCount {
	return s.totalBytesLost
}

// TotalBytesNeutered returns the total bytes neutered.
func (s *BandwidthSampler) TotalBytesNeutered() congestion.ByteCount {
	return s.totalBytesNeutered
}

// IsAppLimited returns whether the sampler is in app-limited phase.
func (s *BandwidthSampler) IsAppLimited() bool {
	return s.isAppLimited
}

// EndOfAppLimitedPhase returns the end of app limited phase packet number.
func (s *BandwidthSampler) EndOfAppLimitedPhase() congestion.PacketNumber {
	return s.endOfAppLimitedPhase
}

// MaxAckHeight returns the maximum ack height.
func (s *BandwidthSampler) MaxAckHeight() congestion.ByteCount {
	return s.maxAckHeightTracker.Get()
}

// NumAckAggregationEpochs returns the number of ack aggregation epochs.
func (s *BandwidthSampler) NumAckAggregationEpochs() uint64 {
	return s.maxAckHeightTracker.NumAckAggregationEpochs()
}

// SetMaxAckHeightTrackerWindowLength sets the window length for the max ack height tracker.
func (s *BandwidthSampler) SetMaxAckHeightTrackerWindowLength(length uint64) {
	s.maxAckHeightTracker.SetFilterWindowLength(length)
}

// ResetMaxAckHeightTracker resets the max ack height tracker.
func (s *BandwidthSampler) ResetMaxAckHeightTracker(newHeight congestion.ByteCount, newTime uint64) {
	s.maxAckHeightTracker.Reset(newHeight, newTime)
}

// SetStartNewAggregationEpochAfterFullRound sets the option.
func (s *BandwidthSampler) SetStartNewAggregationEpochAfterFullRound(value bool) {
	s.maxAckHeightTracker.SetStartNewAggregationEpochAfterFullRound(value)
}

// SetLimitMaxAckHeightTrackerBySendRate sets the option.
func (s *BandwidthSampler) SetLimitMaxAckHeightTrackerBySendRate(value bool) {
	s.limitMaxAckHeightTrackerBySendRate = value
}

// SetReduceExtraAckedOnBandwidthIncrease sets the option.
func (s *BandwidthSampler) SetReduceExtraAckedOnBandwidthIncrease(value bool) {
	s.maxAckHeightTracker.SetReduceExtraAckedOnBandwidthIncrease(value)
}

// EnableOverestimateAvoidance enables overestimate avoidance.
func (s *BandwidthSampler) EnableOverestimateAvoidance() {
	if s.overestimateAvoidance {
		return
	}
	s.overestimateAvoidance = true
	s.maxAckHeightTracker.SetAckAggregationBandwidthThreshold(2.0)
}

// IsOverestimateAvoidanceEnabled returns whether overestimate avoidance is enabled.
func (s *BandwidthSampler) IsOverestimateAvoidanceEnabled() bool {
	return s.overestimateAvoidance
}
