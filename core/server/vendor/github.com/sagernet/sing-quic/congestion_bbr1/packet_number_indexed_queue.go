// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ported from:
// https://github.com/google/quiche/blob/main/quiche/quic/core/packet_number_indexed_queue.h

// PacketNumberIndexedQueue is a queue of mostly continuous numbered entries
// which supports the following operations:
// - adding elements to the end of the queue, or at some point past the end
// - removing elements in any order
// - retrieving elements
// If all elements are inserted in order, all of the operations above are
// amortized O(1) time.

package congestion_bbr1

import "github.com/sagernet/quic-go/congestion"

// entryWrapper wraps an entry to mark whether it's present in the queue.
type entryWrapper[T any] struct {
	value   T
	present bool
}

// PacketNumberIndexedQueue is a queue indexed by packet number.
type PacketNumberIndexedQueue[T any] struct {
	entries                []entryWrapper[T]
	numberOfPresentEntries int
	firstPacket            congestion.PacketNumber
	firstPacketInitialized bool
}

// NewPacketNumberIndexedQueue creates a new PacketNumberIndexedQueue.
func NewPacketNumberIndexedQueue[T any]() *PacketNumberIndexedQueue[T] {
	return &PacketNumberIndexedQueue[T]{}
}

// GetEntry retrieves the entry associated with the packet number.
// Returns a pointer to the entry if present, or nil if not.
func (q *PacketNumberIndexedQueue[T]) GetEntry(packetNumber congestion.PacketNumber) *T {
	if !q.firstPacketInitialized || q.IsEmpty() || packetNumber < q.firstPacket {
		return nil
	}

	offset := int(packetNumber - q.firstPacket)
	if offset >= len(q.entries) {
		return nil
	}

	entry := &q.entries[offset]
	if !entry.present {
		return nil
	}

	return &entry.value
}

// Emplace inserts data associated with packetNumber into (or past) the end of the
// queue, filling up the missing intermediate entries as necessary.
// Returns true if the element has been inserted successfully, false if it was already
// in the queue or inserted out of order.
func (q *PacketNumberIndexedQueue[T]) Emplace(packetNumber congestion.PacketNumber, value T) bool {
	if q.IsEmpty() {
		q.entries = append(q.entries, entryWrapper[T]{value: value, present: true})
		q.numberOfPresentEntries = 1
		q.firstPacket = packetNumber
		q.firstPacketInitialized = true
		return true
	}

	// Do not allow insertion out-of-order.
	if packetNumber <= q.LastPacket() {
		return false
	}

	// Handle potentially missing elements.
	offset := int(packetNumber - q.firstPacket)
	if offset > len(q.entries) {
		// Extend the slice with empty entries
		for i := len(q.entries); i < offset; i++ {
			q.entries = append(q.entries, entryWrapper[T]{present: false})
		}
	}

	q.numberOfPresentEntries++
	q.entries = append(q.entries, entryWrapper[T]{value: value, present: true})
	return true
}

// Remove removes data associated with packetNumber and frees the slots in the
// queue as necessary.
func (q *PacketNumberIndexedQueue[T]) Remove(packetNumber congestion.PacketNumber) bool {
	return q.RemoveWithCallback(packetNumber, nil)
}

// RemoveWithCallback removes data associated with packetNumber and calls f with
// the entry before removing it.
func (q *PacketNumberIndexedQueue[T]) RemoveWithCallback(packetNumber congestion.PacketNumber, f func(*T)) bool {
	if !q.firstPacketInitialized || q.IsEmpty() || packetNumber < q.firstPacket {
		return false
	}

	offset := int(packetNumber - q.firstPacket)
	if offset >= len(q.entries) {
		return false
	}

	entry := &q.entries[offset]
	if !entry.present {
		return false
	}

	if f != nil {
		f(&entry.value)
	}
	entry.present = false
	q.numberOfPresentEntries--

	if packetNumber == q.firstPacket {
		q.cleanup()
	}
	return true
}

// RemoveUpTo removes entries up to, but not including packetNumber.
// Unused slots in the front are also removed.
func (q *PacketNumberIndexedQueue[T]) RemoveUpTo(packetNumber congestion.PacketNumber) {
	for len(q.entries) > 0 && q.firstPacketInitialized && q.firstPacket < packetNumber {
		if q.entries[0].present {
			q.numberOfPresentEntries--
		}
		q.entries = q.entries[1:]
		q.firstPacket++
	}
	q.cleanup()
}

// cleanup cleans up unused slots in the front.
func (q *PacketNumberIndexedQueue[T]) cleanup() {
	for len(q.entries) > 0 && !q.entries[0].present {
		q.entries = q.entries[1:]
		q.firstPacket++
	}
	if len(q.entries) == 0 {
		q.firstPacketInitialized = false
	}
}

// IsEmpty returns true if the queue has no present entries.
func (q *PacketNumberIndexedQueue[T]) IsEmpty() bool {
	return q.numberOfPresentEntries == 0
}

// NumberOfPresentEntries returns the number of entries in the queue.
func (q *PacketNumberIndexedQueue[T]) NumberOfPresentEntries() int {
	return q.numberOfPresentEntries
}

// EntrySlotsUsed returns the number of entries allocated in the underlying slice.
func (q *PacketNumberIndexedQueue[T]) EntrySlotsUsed() int {
	return len(q.entries)
}

// FirstPacket returns the packet number of the first entry in the queue.
func (q *PacketNumberIndexedQueue[T]) FirstPacket() congestion.PacketNumber {
	return q.firstPacket
}

// LastPacket returns the packet number of the last entry ever inserted in the queue.
// Note that the entry in question may have already been removed.
func (q *PacketNumberIndexedQueue[T]) LastPacket() congestion.PacketNumber {
	if q.IsEmpty() {
		return 0
	}
	return q.firstPacket + congestion.PacketNumber(len(q.entries)) - 1
}

// Reserve reserves the specified capacity in the underlying slice.
func (q *PacketNumberIndexedQueue[T]) Reserve(capacity int) {
	if cap(q.entries) < capacity {
		newEntries := make([]entryWrapper[T], len(q.entries), capacity)
		copy(newEntries, q.entries)
		q.entries = newEntries
	}
}
