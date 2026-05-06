// BBRv2 bandwidth type definitions
// src from: https://github.com/cloudflare/quiche

package congestion_bbr2

import (
	"math"
	"time"

	"github.com/sagernet/quic-go/congestion"
)

// Bandwidth represents a data rate in bits per second.
type Bandwidth uint64

const (
	BitsPerSecond   Bandwidth = 1
	BytesPerSecond  Bandwidth = 8 * BitsPerSecond
	KBitsPerSecond  Bandwidth = 1000 * BitsPerSecond
	KBytesPerSecond Bandwidth = 8 * KBitsPerSecond
	MBitsPerSecond  Bandwidth = 1000 * KBitsPerSecond
	MBytesPerSecond Bandwidth = 8 * MBitsPerSecond
	GBitsPerSecond  Bandwidth = 1000 * MBitsPerSecond
	GBytesPerSecond Bandwidth = 8 * GBitsPerSecond

	infBandwidth Bandwidth = math.MaxUint64
)

// BandwidthFromBytesAndTimeDelta creates a Bandwidth from bytes and time delta.
func BandwidthFromBytesAndTimeDelta(bytes congestion.ByteCount, delta time.Duration) Bandwidth {
	if delta <= 0 {
		return infBandwidth
	}
	return Bandwidth(uint64(bytes) * uint64(time.Second) / uint64(delta) * uint64(BytesPerSecond))
}

// BandwidthFromBytesPerSecond creates a Bandwidth from bytes per second.
func BandwidthFromBytesPerSecond(bytesPerSecond uint64) Bandwidth {
	return Bandwidth(bytesPerSecond * uint64(BytesPerSecond))
}

// ToBytesPerSecond converts bandwidth to bytes per second.
func (b Bandwidth) ToBytesPerSecond() uint64 {
	return uint64(b) / uint64(BytesPerSecond)
}

// ToBytesPerPeriod returns the number of bytes that can be transmitted in the given period.
func (b Bandwidth) ToBytesPerPeriod(period time.Duration) congestion.ByteCount {
	return congestion.ByteCount(uint64(b) * uint64(period) / uint64(time.Second) / uint64(BytesPerSecond))
}

// Mul multiplies bandwidth by a float64 factor.
func (b Bandwidth) Mul(factor float64) Bandwidth {
	return Bandwidth(float64(b) * factor)
}

// IsZero returns true if bandwidth is zero.
func (b Bandwidth) IsZero() bool {
	return b == 0
}

// IsInfinite returns true if bandwidth is infinite.
func (b Bandwidth) IsInfinite() bool {
	return b == infBandwidth
}

// BytesFromBandwidthAndTimeDelta calculates bytes from bandwidth and time delta.
func BytesFromBandwidthAndTimeDelta(bandwidth Bandwidth, delta time.Duration) congestion.ByteCount {
	return bandwidth.ToBytesPerPeriod(delta)
}
