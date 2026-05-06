// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ported from:
// https://github.com/google/quiche/blob/main/quiche/quic/core/congestion_control/windowed_filter.h

// Implements Kathleen Nichols' algorithm for tracking the minimum (or maximum)
// estimate of a stream of samples over some fixed time interval. (E.g.,
// the minimum RTT over the past five minutes.) The algorithm keeps track of
// the best, second best, and third best min (or max) estimates, maintaining an
// invariant that the measurement time of the n'th best >= n-1'th best.

package congestion_bbr1

// WindowedFilterValue is the type constraint for values that can be stored in the filter.
type WindowedFilterValue interface {
	~int64 | ~uint64 | ~float64
}

// WindowedFilterTime is the type constraint for time values used in the filter.
type WindowedFilterTime interface {
	~int64 | ~uint64
}

// sample holds a value and its time.
type sample[V WindowedFilterValue, T WindowedFilterTime] struct {
	value V
	time  T
}

// WindowedFilter implements Kathleen Nichols' algorithm for tracking the min/max
// estimate of a stream of samples over some fixed time interval.
type WindowedFilter[V WindowedFilterValue, T WindowedFilterTime] struct {
	windowLength T
	zeroValue    V
	zeroTime     T
	estimates    [3]sample[V, T] // best, secondBest, thirdBest
	comparator   func(V, V) bool // returns true if first arg is "better" than second
	initialized  bool            // tracks whether the filter has been initialized
}

// NewWindowedFilter creates a new WindowedFilter.
// windowLength is the period after which a best estimate expires.
// zeroValue is used as the uninitialized value for objects of V.
// zeroTime is used as the uninitialized value for objects of T.
// comparator returns true if the first argument is "better" than the second.
// For a max filter, use func(a, b V) bool { return a >= b }
// For a min filter, use func(a, b V) bool { return a <= b }
func NewWindowedFilter[V WindowedFilterValue, T WindowedFilterTime](
	windowLength T,
	zeroValue V,
	zeroTime T,
	comparator func(V, V) bool,
) *WindowedFilter[V, T] {
	f := &WindowedFilter[V, T]{
		windowLength: windowLength,
		zeroValue:    zeroValue,
		zeroTime:     zeroTime,
		comparator:   comparator,
	}
	for i := range f.estimates {
		f.estimates[i] = sample[V, T]{value: zeroValue, time: zeroTime}
	}
	return f
}

// NewMaxFilter creates a WindowedFilter configured as a max filter.
func NewMaxFilter[V WindowedFilterValue, T WindowedFilterTime](
	windowLength T,
	zeroValue V,
	zeroTime T,
) *WindowedFilter[V, T] {
	return NewWindowedFilter(windowLength, zeroValue, zeroTime, func(a, b V) bool {
		return a >= b
	})
}

// NewMinFilter creates a WindowedFilter configured as a min filter.
func NewMinFilter[V WindowedFilterValue, T WindowedFilterTime](
	windowLength T,
	zeroValue V,
	zeroTime T,
) *WindowedFilter[V, T] {
	return NewWindowedFilter(windowLength, zeroValue, zeroTime, func(a, b V) bool {
		return a <= b
	})
}

// SetWindowLength changes the window length. Does not update any current samples.
func (f *WindowedFilter[V, T]) SetWindowLength(windowLength T) {
	f.windowLength = windowLength
}

// Update updates best estimates with sample, and expires and updates best
// estimates as necessary.
func (f *WindowedFilter[V, T]) Update(newSample V, newTime T) {
	// Reset all estimates if they have not yet been initialized, if new sample
	// is a new best, or if the newest recorded estimate is too old.
	if !f.initialized ||
		f.comparator(newSample, f.estimates[0].value) ||
		newTime-f.estimates[2].time > f.windowLength {
		f.Reset(newSample, newTime)
		return
	}

	if f.comparator(newSample, f.estimates[1].value) {
		f.estimates[1] = sample[V, T]{value: newSample, time: newTime}
		f.estimates[2] = f.estimates[1]
	} else if f.comparator(newSample, f.estimates[2].value) {
		f.estimates[2] = sample[V, T]{value: newSample, time: newTime}
	}

	// Expire and update estimates as necessary.
	if newTime-f.estimates[0].time > f.windowLength {
		// The best estimate hasn't been updated for an entire window, so promote
		// second and third best estimates.
		f.estimates[0] = f.estimates[1]
		f.estimates[1] = f.estimates[2]
		f.estimates[2] = sample[V, T]{value: newSample, time: newTime}
		// Need to iterate one more time. Check if the new best estimate is
		// outside the window as well, since it may also have been recorded a
		// long time ago. Don't need to iterate once more since we cover that
		// case at the beginning of the method.
		if newTime-f.estimates[0].time > f.windowLength {
			f.estimates[0] = f.estimates[1]
			f.estimates[1] = f.estimates[2]
		}
		return
	}

	if f.estimates[1].value == f.estimates[0].value &&
		newTime-f.estimates[1].time > f.windowLength>>2 {
		// A quarter of the window has passed without a better sample, so the
		// second-best estimate is taken from the second quarter of the window.
		f.estimates[1] = sample[V, T]{value: newSample, time: newTime}
		f.estimates[2] = f.estimates[1]
		return
	}

	if f.estimates[2].value == f.estimates[1].value &&
		newTime-f.estimates[2].time > f.windowLength>>1 {
		// We've passed a half of the window without a better estimate, so take
		// a third-best estimate from the second half of the window.
		f.estimates[2] = sample[V, T]{value: newSample, time: newTime}
	}
}

// Reset resets all estimates to new sample.
func (f *WindowedFilter[V, T]) Reset(newSample V, newTime T) {
	f.estimates[0] = sample[V, T]{value: newSample, time: newTime}
	f.estimates[1] = f.estimates[0]
	f.estimates[2] = f.estimates[0]
	f.initialized = true
}

// Clear clears all estimates.
func (f *WindowedFilter[V, T]) Clear() {
	f.Reset(f.zeroValue, f.zeroTime)
	f.initialized = false
}

// GetBest returns the best estimate.
func (f *WindowedFilter[V, T]) GetBest() V {
	return f.estimates[0].value
}

// GetSecondBest returns the second best estimate.
func (f *WindowedFilter[V, T]) GetSecondBest() V {
	return f.estimates[1].value
}

// GetThirdBest returns the third best estimate.
func (f *WindowedFilter[V, T]) GetThirdBest() V {
	return f.estimates[2].value
}
