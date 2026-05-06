// Windowed filter implementation for tracking min/max values over time.
// Implements Kathleen Nichols' algorithm for tracking the minimum (or maximum)
// estimate of a stream of samples over some fixed time interval.
// src from: https://github.com/cloudflare/quiche/blob/master/quiche/src/recovery/gcongestion/bbr/windowed_filter.rs

package congestion_bbr2

// RoundTripCount is used as window length unit for bandwidth filters.
type RoundTripCount int64

// bwFilterSample holds a bandwidth value and the round it was recorded.
type bwFilterSample struct {
	value Bandwidth
	round RoundTripCount
}

// WindowedFilter tracks min or max bandwidth values over a sliding window.
// It keeps track of the best, second best, and third best estimates.
type WindowedFilter struct {
	windowLength RoundTripCount
	estimates    [3]*bwFilterSample
	// compare returns true if a is "better" than b (> for max filter, < for min filter)
	compare func(a, b Bandwidth) bool
}

// NewMaxFilter creates a filter that tracks maximum values.
func NewMaxFilter(windowLength RoundTripCount) *WindowedFilter {
	return &WindowedFilter{
		windowLength: windowLength,
		compare:      func(a, b Bandwidth) bool { return a > b },
	}
}

// NewMinFilter creates a filter that tracks minimum values.
func NewMinFilter(windowLength RoundTripCount) *WindowedFilter {
	return &WindowedFilter{
		windowLength: windowLength,
		compare:      func(a, b Bandwidth) bool { return a < b },
	}
}

// Reset sets all three estimates to the same sample.
func (f *WindowedFilter) Reset(newSample Bandwidth, newRound RoundTripCount) {
	s := &bwFilterSample{value: newSample, round: newRound}
	f.estimates[0] = s
	f.estimates[1] = s
	f.estimates[2] = s
}

// GetBest returns the best estimate, or zero if not set.
func (f *WindowedFilter) GetBest() Bandwidth {
	if f.estimates[0] == nil {
		return 0
	}
	return f.estimates[0].value
}

// GetSecondBest returns the second best estimate, or zero if not set.
func (f *WindowedFilter) GetSecondBest() Bandwidth {
	if f.estimates[1] == nil {
		return 0
	}
	return f.estimates[1].value
}

// GetThirdBest returns the third best estimate, or zero if not set.
func (f *WindowedFilter) GetThirdBest() Bandwidth {
	if f.estimates[2] == nil {
		return 0
	}
	return f.estimates[2].value
}

// HasValue returns true if the filter has been initialized.
func (f *WindowedFilter) HasValue() bool {
	return f.estimates[0] != nil
}

// Clear resets the filter to uninitialized state.
func (f *WindowedFilter) Clear() {
	f.estimates[0] = nil
	f.estimates[1] = nil
	f.estimates[2] = nil
}

// Update adds a new sample to the filter.
func (f *WindowedFilter) Update(newSample Bandwidth, newRound RoundTripCount) {
	// Reset all estimates if they have not yet been initialized, if new
	// sample is a new best, or if the newest recorded estimate is too old.
	if f.estimates[0] == nil || f.estimates[2] == nil ||
		f.compare(newSample, f.estimates[0].value) ||
		newRound-f.estimates[2].round > f.windowLength {
		f.Reset(newSample, newRound)
		return
	}

	if f.compare(newSample, f.estimates[1].value) {
		s := &bwFilterSample{value: newSample, round: newRound}
		f.estimates[1] = s
		f.estimates[2] = s
	} else if f.compare(newSample, f.estimates[2].value) {
		f.estimates[2] = &bwFilterSample{value: newSample, round: newRound}
	}

	// Expire and update estimates as necessary.
	if newRound-f.estimates[0].round > f.windowLength {
		// The best estimate hasn't been updated for an entire window, so
		// promote second and third best estimates.
		f.estimates[0] = f.estimates[1]
		f.estimates[1] = f.estimates[2]
		f.estimates[2] = &bwFilterSample{value: newSample, round: newRound}
		// Need to iterate one more time. Check if the new best estimate is
		// outside the window as well, since it may also have been recorded a
		// long time ago.
		if newRound-f.estimates[0].round > f.windowLength {
			f.estimates[0] = f.estimates[1]
			f.estimates[1] = f.estimates[2]
		}
		return
	}

	quarterWindow := f.windowLength / 4
	if f.estimates[1].value == f.estimates[0].value &&
		newRound-f.estimates[1].round > quarterWindow {
		// A quarter of the window has passed without a better sample, so the
		// second-best estimate is taken from the second quarter of the window.
		s := &bwFilterSample{value: newSample, round: newRound}
		f.estimates[1] = s
		f.estimates[2] = s
		return
	}

	halfWindow := f.windowLength / 2
	if f.estimates[2].value == f.estimates[1].value &&
		newRound-f.estimates[2].round > halfWindow {
		// We've passed a half of the window without a better estimate, so
		// take a third-best estimate from the second half of the window.
		f.estimates[2] = &bwFilterSample{value: newSample, round: newRound}
	}
}
