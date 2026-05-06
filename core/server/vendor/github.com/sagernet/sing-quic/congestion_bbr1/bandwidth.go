// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ported from:
// https://github.com/google/quiche/blob/main/quiche/quic/core/quic_bandwidth.h

package congestion_bbr1

import (
	"math"
	"time"

	"github.com/sagernet/quic-go/congestion"
)

// Bandwidth represents a bandwidth, stored in bits per second resolution.
type Bandwidth int64

const (
	// BitsPerSecond is 1 bit per second.
	BitsPerSecond Bandwidth = 1
	// BytesPerSecond is 8 bits per second.
	BytesPerSecond = 8 * BitsPerSecond
	// KBitsPerSecond is 1000 bits per second.
	KBitsPerSecond = 1000 * BitsPerSecond
	// KBytesPerSecond is 8000 bits per second.
	KBytesPerSecond = 8000 * BitsPerSecond
)

const (
	infiniteBandwidth = Bandwidth(math.MaxInt64)
)

// BandwidthFromBytesAndTimeDelta creates a new Bandwidth based on the bytes per the elapsed delta.
func BandwidthFromBytesAndTimeDelta(bytes congestion.ByteCount, delta time.Duration) Bandwidth {
	if bytes == 0 {
		return 0
	}
	if delta <= 0 {
		return infiniteBandwidth
	}
	// 1 bit is 1000000 micro bits.
	numMicroBits := int64(bytes) * 8 * int64(time.Second/time.Microsecond)
	deltaMicros := delta.Microseconds()
	if numMicroBits < deltaMicros {
		return 1
	}
	return Bandwidth(numMicroBits / deltaMicros)
}

// Zero returns a zero bandwidth.
func (b Bandwidth) Zero() Bandwidth {
	return 0
}

// IsZero returns true if the bandwidth is zero.
func (b Bandwidth) IsZero() bool {
	return b == 0
}

// IsInfinite returns true if the bandwidth is infinite.
func (b Bandwidth) IsInfinite() bool {
	return b == infiniteBandwidth
}

// ToBitsPerSecond returns the bandwidth in bits per second.
func (b Bandwidth) ToBitsPerSecond() int64 {
	return int64(b)
}

// ToBytesPerSecond returns the bandwidth in bytes per second.
func (b Bandwidth) ToBytesPerSecond() int64 {
	return int64(b) / 8
}

// ToKBitsPerSecond returns the bandwidth in kilo bits per second.
func (b Bandwidth) ToKBitsPerSecond() int64 {
	return int64(b) / 1000
}

// ToKBytesPerSecond returns the bandwidth in kilo bytes per second.
func (b Bandwidth) ToKBytesPerSecond() int64 {
	return int64(b) / 8000
}

// ToBytesPerPeriod returns the number of bytes that can be transmitted in the given time period.
func (b Bandwidth) ToBytesPerPeriod(timePeriod time.Duration) congestion.ByteCount {
	return congestion.ByteCount(int64(b) * timePeriod.Microseconds() / 8 / int64(time.Second/time.Microsecond))
}

// TransferTime returns the time it takes to transfer the given number of bytes.
func (b Bandwidth) TransferTime(bytes congestion.ByteCount) time.Duration {
	if b == 0 {
		return 0
	}
	return time.Duration(int64(bytes) * 8 * int64(time.Second/time.Microsecond) / int64(b) * int64(time.Microsecond))
}

// Infinite returns an infinite bandwidth value.
func InfiniteBandwidth() Bandwidth {
	return infiniteBandwidth
}
