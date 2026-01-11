package congestion

import (
	"time"

	"github.com/sagernet/quic-go/congestion"
)

const (
	maxBurstPackets               = 10
	maxBurstPacingDelayMultiplier = 4
	minPacingDelay                = time.Millisecond
)

// The pacer implements a token bucket pacing algorithm.
type pacer struct {
	budgetAtLastSent congestion.ByteCount
	maxDatagramSize  congestion.ByteCount
	lastSentTime     time.Time
	getBandwidth     func() congestion.ByteCount // in bytes/s
}

func newPacer(getBandwidth func() congestion.ByteCount) *pacer {
	p := &pacer{
		budgetAtLastSent: maxBurstPackets * initMaxDatagramSize,
		maxDatagramSize:  initMaxDatagramSize,
		getBandwidth:     getBandwidth,
	}
	return p
}

func (p *pacer) SentPacket(sendTime time.Time, size congestion.ByteCount) {
	budget := p.Budget(sendTime)
	if size > budget {
		p.budgetAtLastSent = 0
	} else {
		p.budgetAtLastSent = budget - size
	}
	p.lastSentTime = sendTime
}

func (p *pacer) Budget(now time.Time) congestion.ByteCount {
	if p.lastSentTime.IsZero() {
		return p.maxBurstSize()
	}
	budget := p.budgetAtLastSent + (p.getBandwidth()*congestion.ByteCount(now.Sub(p.lastSentTime).Nanoseconds()))/1e9
	if budget < 0 { // protect against overflows
		budget = congestion.ByteCount(1<<62 - 1)
	}
	return minByteCount(p.maxBurstSize(), budget)
}

func (p *pacer) maxBurstSize() congestion.ByteCount {
	return maxByteCount(
		congestion.ByteCount((maxBurstPacingDelayMultiplier*minPacingDelay).Nanoseconds())*p.getBandwidth()/1e9,
		maxBurstPackets*p.maxDatagramSize,
	)
}

// TimeUntilSend returns when the next packet should be sent.
// It returns the zero value of time.Time if a packet can be sent immediately.
func (p *pacer) TimeUntilSend() time.Time {
	if p.budgetAtLastSent >= p.maxDatagramSize {
		return time.Time{}
	}
	diff := 1e9 * uint64(p.maxDatagramSize-p.budgetAtLastSent)
	bw := uint64(p.getBandwidth())
	// We might need to round up this value.
	// Otherwise, we might have a budget (slightly) smaller than the datagram size when the timer expires.
	d := diff / bw
	// this is effectively a math.Ceil, but using only integer math
	if diff%bw > 0 {
		d++
	}
	return p.lastSentTime.Add(maxDuration(congestion.MinPacingDelay, time.Duration(d)*time.Nanosecond))
}

func (p *pacer) SetMaxDatagramSize(s congestion.ByteCount) {
	p.maxDatagramSize = s
}

func maxByteCount(a, b congestion.ByteCount) congestion.ByteCount {
	if a < b {
		return b
	}
	return a
}

func minByteCount(a, b congestion.ByteCount) congestion.ByteCount {
	if a < b {
		return a
	}
	return b
}
