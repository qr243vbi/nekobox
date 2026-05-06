// Copyright (c) 2024 Karl Gaissmaier
// SPDX-License-Identifier: MIT

// Package bitset implements bitsets, a mapping
// between non-negative integers and boolean values.
//
// Studied [github.com/bits-and-blooms/bitset] inside out
// and rewrote needed parts from scratch for this project.
//
// This implementation is smaller and faster as the more
// general [github.com/bits-and-blooms/bitset].
//
// All functions can be inlined!
//
//	can inline BitSet.Set with cost 63
//	can inline BitSet.Clear with cost 24
//	can inline BitSet.Test with cost 26
//	can inline BitSet.Rank0 with cost 66
//	can inline BitSet.Clone with cost 7
//	can inline BitSet.Compact with cost 35
//	can inline BitSet.FirstSet with cost 25
//	can inline BitSet.NextSet with cost 71
//	can inline BitSet.AsSlice with cost 50
//	can inline BitSet.All with cost 62
//	can inline BitSet.IntersectsAny with cost 42
//	can inline BitSet.IntersectionTop with cost 56
//	can inline BitSet.IntersectionCardinality with cost 35
//	can inline (*BitSet).InPlaceIntersection with cost 71
//	can inline (*BitSet).InPlaceUnion with cost 77
//	can inline BitSet.Size with cost 16
//	can inline popcount with cost 12
//	can inline popcountAnd with cost 30
package bitset

import (
	"math/bits"
)

// A BitSet is a slice of words. This is an internal package
// with a wide open public API.
type BitSet []uint64

//   xIdx calculates the index of i in a []uint64
//   func wIdx(i uint) int {
//   	return int(i >> 6) // like (i / 64) but faster
//   }

//   bIdx calculates the index of i in a `uint64`
//   func bIdx(i uint) uint {
//   	return i & 63 // like (i % 64) but faster
//   }
//
// just as an explanation of the expressions,
//
//   i>>6 or i<<6 and i&63
//
// not factored out as functions to make most of the methods
// inlineable with minimal costs.

// Set bit i to 1, the capacity of the bitset is increased accordingly.
func (b BitSet) Set(i uint) BitSet {
	// grow?
	if i >= uint(len(b)<<6) {
		words := int((i + 64) >> 6)
		switch {
		case b == nil:
			b = make([]uint64, words)
		case cap(b) >= words:
			b = b[:words]
		default:
			// be exact, don't use append!
			// max 512 prefixes/node (8*uint64), and a cache line has 64 Bytes
			newset := make([]uint64, words)
			copy(newset, b)
			b = newset
		}
	}

	b[i>>6] |= 1 << (i & 63)
	return b
}

// Clear bit i to 0.
func (b BitSet) Clear(i uint) BitSet {
	if x := int(i >> 6); x < len(b) {
		b[x] &^= 1 << (i & 63)
	}
	return b
}

// Test if bit i is set.
func (b BitSet) Test(i uint) (ok bool) {
	if x := int(i >> 6); x < len(b) {
		return b[x]&(1<<(i&63)) != 0
	}
	return
}

// Clone this BitSet, returning a new BitSet that has the same bits set.
func (b BitSet) Clone() BitSet {
	return append(b[:0:0], b...)
}

// Compact, preserve all set bits, while minimizing memory usage.
func (b BitSet) Compact() BitSet {
	last := len(b) - 1

	// find last word with at least one bit set.
	for ; last >= 0; last-- {
		if b[last] != 0 {
			b = b[: last+1 : last+1]
			return b
		}
	}

	// BitSet was empty, shrink to nil
	return nil
}

// FirstSet returns the first bit set along with an ok code.
func (b BitSet) FirstSet() (uint, bool) {
	for x, word := range b {
		if word != 0 {
			return uint(x<<6 + bits.TrailingZeros64(word)), true
		}
	}
	return 0, false
}

// NextSet returns the next bit set from the specified index,
// including possibly the current index along with an ok code.
func (b BitSet) NextSet(i uint) (uint, bool) {
	x := int(i >> 6)
	if x >= len(b) {
		return 0, false
	}

	// process the first (maybe partial) word
	first := b[x] >> (i & 63) // i % 64
	if first != 0 {
		return i + uint(bits.TrailingZeros64(first)), true
	}

	// process the following words until next bit is set
	// x < len(b), no out-of-bounds panic in following slice expression
	x++
	for j, word := range b[x:] {
		if word != 0 {
			return uint((x+j)<<6 + bits.TrailingZeros64(word)), true
		}
	}
	return 0, false
}

// AsSlice returns all set bits as slice of uint without
// heap allocations.
//
// This is faster than All, but also more dangerous,
// it panics if the capacity of buf is < b.Size()
func (b BitSet) AsSlice(buf []uint) []uint {
	buf = buf[:cap(buf)] // len = cap

	size := 0
	for idx, word := range b {
		for ; word != 0; size++ {
			// panics if capacity of buf is exceeded.
			buf[size] = uint(idx<<6 + bits.TrailingZeros64(word))

			// clear the rightmost set bit
			word &= word - 1
		}
	}

	buf = buf[:size]
	return buf
}

// All returns all set bits. This is simpler but slower than AsSlice.
func (b BitSet) All() []uint {
	buf := make([]uint, b.Size())

	slot := 0
	for idx, word := range b {
		for word != 0 {
			buf[slot] = uint(idx<<6 + bits.TrailingZeros64(word))
			slot++

			// clear the rightmost set bit
			word &= word - 1
		}
	}

	return buf
}

// IntersectsAny returns true if the intersection of base set with the compare set
// is not the empty set.
func (b BitSet) IntersectsAny(c BitSet) bool {
	i := min(len(b), len(c)) - 1
	// bounds check eliminated (BCE)
	for ; i >= 0 && i < len(b) && i < len(c); i-- {
		if b[i]&c[i] != 0 {
			return true
		}
	}
	return false
}

// IntersectionTop computes the intersection of base set with the compare set.
// If the result set isn't empty, it returns the top most set bit and true.
func (b BitSet) IntersectionTop(c BitSet) (top uint, ok bool) {
	i := min(len(b), len(c)) - 1
	// bounds check eliminated (BCE)
	for ; i >= 0 && i < len(b) && i < len(c); i-- {
		if word := b[i] & c[i]; word != 0 {
			return uint(i<<6+bits.Len64(word)) - 1, true
		}
	}
	return
}

// IntersectionCardinality computes the popcount of the intersection.
func (b BitSet) IntersectionCardinality(c BitSet) int {
	return popcntAnd(b, c)
}

// InPlaceIntersection overwrites and computes the intersection of
// base set with the compare set. This is the BitSet equivalent of & (and).
// If len(c) > len(b), new memory is allocated.
func (b *BitSet) InPlaceIntersection(c BitSet) {
	// bounds check eliminated, range until minLen(b,c)
	for i := 0; i < len(*b) && i < len(c); i++ {
		(*b)[i] &= c[i]
	}

	// b >= c
	if len(*b) >= len(c) {
		// bounds check eliminated
		for i := len(c); i < len(*b); i++ {
			(*b)[i] = 0
		}
		return
	}

	// b < c
	newset := make([]uint64, len(c))
	copy(newset, *b)
	*b = newset
}

// InPlaceUnion creates the destructive union of base set with compare set.
// This is the BitSet equivalent of | (or).
// If len(c) > len(b), new memory is allocated.
func (b *BitSet) InPlaceUnion(c BitSet) {
	// b >= c
	if len(*b) >= len(c) {
		// bounds check eliminated
		for i := 0; i < len(*b) && i < len(c); i++ {
			(*b)[i] |= c[i]
		}

		return
	}

	// b < c
	newset := make([]uint64, len(c))
	copy(newset, *b)
	*b = newset

	// bounds check eliminated
	for i := 0; i < len(*b) && i < len(c); i++ {
		(*b)[i] |= c[i]
	}
}

// Size (number of set bits).
func (b BitSet) Size() int {
	return popcntSlice(b)
}

// Rank0 is equal to Rank(i) - 1
//
// With inlined popcount to make Rank0 itself inlineable.
func (b BitSet) Rank0(i uint) (rnk int) {
	// Rank count is inclusive
	i++

	if wordIdx := int(i >> 6); wordIdx >= len(b) {
		// inlined popcount, whole slice
		for _, x := range b {
			rnk += bits.OnesCount64(x)
		}
	} else {
		// inlined popcount, partial slice ...
		for _, x := range b[:wordIdx] {
			rnk += bits.OnesCount64(x)
		}

		// ... plus partial word?
		if bitsIdx := i & 63; bitsIdx != 0 {
			rnk += bits.OnesCount64(b[wordIdx] << (64 - bitsIdx))
		}

	}

	// correct for offset by one
	return rnk - 1
}

// popcntSlice
func popcntSlice(s []uint64) (cnt int) {
	for _, x := range s {
		// count all the bits set in slice.
		cnt += bits.OnesCount64(x)
	}
	return
}

// popcntAnd
func popcntAnd(s, m []uint64) (cnt int) {
	for j := 0; j < len(s) && j < len(m); j++ {
		// words are bitwise & followed by popcount.
		cnt += bits.OnesCount64(s[j] & m[j])
	}
	return
}
