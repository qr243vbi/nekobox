// Copyright (c) 2024 Karl Gaissmaier
// SPDX-License-Identifier: MIT

// package sparse implements a generic sparse array
// with popcount compression.
package sparse

import (
	"github.com/gaissmai/bart/internal/bitset"
)

// Array, a generic implementation of a sparse array
// with popcount compression and payload T.
type Array[T any] struct {
	bitset.BitSet
	Items []T
}

// Get the value at i from sparse array.
//
// example: Array.Get(5) -> Array.Items[1]
//
//	                   ⬇
//	BitSet: [0|0|1|0|0|1|0|1|...] <- 3 bits set
//	Items:  [*|*|*]               <- len(Items) = 3
//	           ⬆
//
//	BitSet.Test(5):     true
//	BitSet.popcount(5): 2, for interval [0,5]
//	BitSet.Rank0(5):    1, equal popcount(5)-1
func (s *Array[T]) Get(i uint) (value T, ok bool) {
	if s.Test(i) {
		return s.Items[s.Rank0(i)], true
	}
	return
}

// MustGet, use it only after a successful test
// or the behavior is undefined, maybe it panics.
func (s *Array[T]) MustGet(i uint) T {
	return s.Items[s.Rank0(i)]
}

// UpdateAt or set the value at i via callback. The new value is returned
// and true if the value was already present.
func (s *Array[T]) UpdateAt(i uint, cb func(T, bool) T) (newValue T, wasPresent bool) {
	var rank0 int

	// if already set, get current value
	var oldValue T

	if wasPresent = s.Test(i); wasPresent {
		rank0 = s.Rank0(i)
		oldValue = s.Items[rank0]
	}

	// callback function to get updated or new value
	newValue = cb(oldValue, wasPresent)

	// already set, update and return value
	if wasPresent {
		s.Items[rank0] = newValue

		return newValue, wasPresent
	}

	// new value, insert into bitset ...
	s.BitSet = s.Set(i)

	// bitset has changed, recalc rank
	rank0 = s.Rank0(i)

	// ... and insert value into slice
	s.insertItem(rank0, newValue)

	return newValue, wasPresent
}

// Len returns the number of items in sparse array.
func (s *Array[T]) Len() int {
	return len(s.Items)
}

// Copy returns a shallow copy of the Array.
// The elements are copied using assignment, this is no deep clone.
func (s *Array[T]) Copy() *Array[T] {
	if s == nil {
		return nil
	}

	return &Array[T]{
		BitSet: s.BitSet.Clone(),
		Items:  append(s.Items[:0:0], s.Items...),
	}
}

// InsertAt a value at i into the sparse array.
// If the value already exists, overwrite it with val and return true.
func (s *Array[T]) InsertAt(i uint, value T) (exists bool) {
	// slot exists, overwrite value
	if s.Len() != 0 && s.Test(i) {
		s.Items[s.Rank0(i)] = value

		return true
	}

	// new, insert into bitset ...
	s.BitSet = s.Set(i)

	// ... and slice
	s.insertItem(s.Rank0(i), value)

	return false
}

// DeleteAt a value at i from the sparse array, zeroes the tail.
func (s *Array[T]) DeleteAt(i uint) (value T, exists bool) {
	if s.Len() == 0 || !s.Test(i) {
		return
	}

	rank0 := s.Rank0(i)
	value = s.Items[rank0]

	// delete from slice
	s.deleteItem(rank0)

	// delete from bitset
	s.BitSet = s.Clear(i)

	return value, true
}

// insertItem inserts the item at index i, shift the rest one pos right
//
// It panics if i is out of range.
func (s *Array[T]) insertItem(i int, item T) {
	if len(s.Items) < cap(s.Items) {
		s.Items = s.Items[:len(s.Items)+1] // fast resize, no alloc
	} else {
		var zero T
		s.Items = append(s.Items, zero) // append one item, mostly enlarge cap by more than one item
	}

	copy(s.Items[i+1:], s.Items[i:])
	s.Items[i] = item
}

// deleteItem at index i, shift the rest one pos left and clears the tail item
//
// It panics if i is out of range.
func (s *Array[T]) deleteItem(i int) {
	var zero T

	nl := len(s.Items) - 1           // new len
	copy(s.Items[i:], s.Items[i+1:]) // overwrite item at [i]

	s.Items[nl] = zero     // clear the tail item
	s.Items = s.Items[:nl] // new len, keep cap is unchanged
}
