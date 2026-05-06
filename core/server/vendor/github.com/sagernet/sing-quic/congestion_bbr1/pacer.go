// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package congestion_bbr1

import (
	"time"

	"github.com/sagernet/quic-go/congestion"
	"github.com/sagernet/quic-go/monotime"
)

const (
	// maxBurstPackets is the maximum number of packets that can be sent in a burst.
	maxBurstPackets = 10
	// minPacingDelay is the minimum delay between packets.
	minPacingDelay = time.Millisecond
)

// Pacer implements a token bucket based pacing algorithm.
type Pacer struct {
	budgetAtLastSent congestion.ByteCount
	maxDatagramSize  congestion.ByteCount
	lastSentTime     monotime.Time
	getBandwidth     func() Bandwidth
}

// NewPacer creates a new Pacer.
func NewPacer(getBandwidth func() Bandwidth) *Pacer {
	return &Pacer{
		getBandwidth:    getBandwidth,
		maxDatagramSize: congestion.InitialPacketSize,
	}
}

// SetMaxDatagramSize sets the maximum datagram size.
func (p *Pacer) SetMaxDatagramSize(size congestion.ByteCount) {
	p.maxDatagramSize = size
}

// Budget returns the number of bytes that can be sent at the given time.
func (p *Pacer) Budget(now monotime.Time) congestion.ByteCount {
	if p.lastSentTime.IsZero() {
		return p.maxBurstSize()
	}

	budget := p.budgetAtLastSent + p.bytesForInterval(now.Sub(p.lastSentTime))
	if budget > p.maxBurstSize() {
		budget = p.maxBurstSize()
	}
	return budget
}

// TimeUntilSend returns the time until the next packet can be sent.
// It returns zero if a packet can be sent immediately.
// Note: bytesInFlight is intentionally not used. BBR uses pacing based on
// bandwidth estimate rather than bytes in flight for timing decisions.
// This matches quic-go's pacer design and other BBR implementations (meta2, bbr2).
func (p *Pacer) TimeUntilSend() monotime.Time {
	if p.lastSentTime.IsZero() || p.budgetAtLastSent >= p.maxDatagramSize {
		return 0
	}
	return p.lastSentTime.Add(p.intervalForBytes(p.maxDatagramSize - p.budgetAtLastSent))
}

// OnPacketSent is called when a packet is sent.
func (p *Pacer) OnPacketSent(sentTime monotime.Time, size congestion.ByteCount) {
	if !p.lastSentTime.IsZero() {
		p.budgetAtLastSent = p.Budget(sentTime)
	}
	p.lastSentTime = sentTime
	if size > p.budgetAtLastSent {
		p.budgetAtLastSent = 0
	} else {
		p.budgetAtLastSent -= size
	}
}

// maxBurstSize returns the maximum burst size.
func (p *Pacer) maxBurstSize() congestion.ByteCount {
	return maxBurstPackets * p.maxDatagramSize
}

// bytesForInterval returns the number of bytes that can be sent in the given interval.
func (p *Pacer) bytesForInterval(interval time.Duration) congestion.ByteCount {
	bandwidth := p.getBandwidth()
	if bandwidth.IsZero() || bandwidth.IsInfinite() {
		return p.maxBurstSize()
	}
	return bandwidth.ToBytesPerPeriod(interval)
}

// intervalForBytes returns the interval needed to send the given number of bytes.
func (p *Pacer) intervalForBytes(bytes congestion.ByteCount) time.Duration {
	bandwidth := p.getBandwidth()
	if bandwidth.IsZero() || bandwidth.IsInfinite() {
		return 0
	}
	interval := bandwidth.TransferTime(bytes)
	if interval < minPacingDelay {
		return minPacingDelay
	}
	return interval
}
