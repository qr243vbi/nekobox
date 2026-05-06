// BBRv2 pacer implementation based on quic-go's token bucket pacer.
// This pacer uses BBRv2's calculated pacing rate instead of bandwidth estimate.

package congestion_bbr2

import (
	"time"

	"github.com/sagernet/quic-go/congestion"
	"github.com/sagernet/quic-go/monotime"
)

const (
	maxBurstSizePackets = 10
	// MinPacingDelay is the minimum delay between sending packets.
	minPacingDelay = time.Millisecond
	// TimerGranularity is the assumed timer granularity.
	timerGranularity = time.Millisecond
)

// pacer implements a token bucket pacing algorithm for BBRv2.
type pacer struct {
	budgetAtLastSent congestion.ByteCount
	maxDatagramSize  congestion.ByteCount
	lastSentTime     monotime.Time
	getPacingRate    func() Bandwidth
}

func newPacer(getPacingRate func() Bandwidth, maxDatagramSize congestion.ByteCount) *pacer {
	p := &pacer{
		maxDatagramSize: maxDatagramSize,
		getPacingRate:   getPacingRate,
	}
	p.budgetAtLastSent = p.maxBurstSize()
	return p
}

// SentPacket should be called when a packet is sent.
func (p *pacer) SentPacket(sendTime monotime.Time, size congestion.ByteCount) {
	budget := p.Budget(sendTime)
	if size >= budget {
		p.budgetAtLastSent = 0
	} else {
		p.budgetAtLastSent = budget - size
	}
	p.lastSentTime = sendTime
}

// Budget returns the current pacing budget in bytes.
func (p *pacer) Budget(now monotime.Time) congestion.ByteCount {
	if p.lastSentTime.IsZero() {
		return p.maxBurstSize()
	}
	delta := now.Sub(p.lastSentTime)
	if delta <= 0 {
		return p.budgetAtLastSent
	}

	// Calculate bytes that can be sent based on pacing rate
	pacingRate := p.getPacingRate()
	if pacingRate.IsZero() {
		return p.maxBurstSize()
	}

	// Use 1.25x pacing rate to avoid under-utilization due to RTT variations
	// This matches quic-go's approach
	adjustedRate := pacingRate.Mul(1.25)
	added := adjustedRate.ToBytesPerPeriod(delta)

	budget := p.budgetAtLastSent + added
	// Check for overflow
	if added > 0 && budget < p.budgetAtLastSent {
		return p.maxBurstSize()
	}
	return min(p.maxBurstSize(), budget)
}

// maxBurstSize returns the maximum burst size in bytes.
func (p *pacer) maxBurstSize() congestion.ByteCount {
	pacingRate := p.getPacingRate()
	if pacingRate.IsZero() {
		return congestion.ByteCount(maxBurstSizePackets) * p.maxDatagramSize
	}

	// Maximum burst is based on pacing rate * (MinPacingDelay + TimerGranularity)
	// or maxBurstSizePackets * maxDatagramSize, whichever is larger
	minDelay := minPacingDelay + timerGranularity
	bwBurst := pacingRate.Mul(1.25).ToBytesPerPeriod(minDelay)
	packetBurst := congestion.ByteCount(maxBurstSizePackets) * p.maxDatagramSize
	return max(bwBurst, packetBurst)
}

// TimeUntilSend returns when the next packet can be sent.
// It returns zero if a packet can be sent immediately.
func (p *pacer) TimeUntilSend() monotime.Time {
	if p.budgetAtLastSent >= p.maxDatagramSize {
		return 0
	}

	needed := p.maxDatagramSize - p.budgetAtLastSent
	pacingRate := p.getPacingRate()
	if pacingRate.IsZero() {
		return 0
	}

	// Calculate wait time based on pacing rate (with 1.25x adjustment)
	adjustedRate := pacingRate.Mul(1.25)
	bytesPerSecond := adjustedRate.ToBytesPerSecond()
	if bytesPerSecond == 0 {
		return 0
	}

	// waitTime = needed / bytesPerSecond (in seconds)
	// Convert to nanoseconds for Duration
	waitNs := uint64(needed) * uint64(time.Second) / bytesPerSecond
	waitDuration := time.Duration(waitNs)

	if waitDuration < minPacingDelay {
		waitDuration = minPacingDelay
	}

	return p.lastSentTime.Add(waitDuration)
}

// SetMaxDatagramSize updates the maximum datagram size.
func (p *pacer) SetMaxDatagramSize(s congestion.ByteCount) {
	p.maxDatagramSize = s
}

func min(a, b congestion.ByteCount) congestion.ByteCount {
	if a < b {
		return a
	}
	return b
}

func max(a, b congestion.ByteCount) congestion.ByteCount {
	if a > b {
		return a
	}
	return b
}
