package shadowaead_2022

const (
	swBlockBitLog = 6                  // 1<<6 == 64 bits
	swBlockBits   = 1 << swBlockBitLog // must be power of 2
	swRingBlocks  = 1 << 7             // must be power of 2
	swBlockMask   = swRingBlocks - 1
	swBitMask     = swBlockBits - 1
	swSize        = (swRingBlocks - 1) * swBlockBits
)

// SlidingWindow maintains a sliding window of uint64 counters.
type SlidingWindow struct {
	last uint64
	ring [swRingBlocks]uint64
}

// Reset resets the filter to its initial state.
func (f *SlidingWindow) Reset() {
	f.last = 0
	f.ring[0] = 0
}

// Check checks whether counter can be accepted by the sliding window filter.
func (f *SlidingWindow) Check(counter uint64) bool {
	switch {
	case counter > f.last: // ahead of window
		return true
	case f.last-counter > swSize: // behind window
		return false
	}

	// In window. Check bit.
	blockIndex := counter >> swBlockBitLog & swBlockMask
	bitIndex := counter & swBitMask
	return f.ring[blockIndex]>>bitIndex&1 == 0
}

// Add adds counter to the sliding window without checking if the counter is valid.
// Call Check beforehand to make sure the counter is valid.
func (f *SlidingWindow) Add(counter uint64) {
	blockIndex := counter >> swBlockBitLog

	// Check if counter is ahead of window.
	if counter > f.last {
		lastBlockIndex := f.last >> swBlockBitLog
		diff := int(blockIndex - lastBlockIndex)
		if diff > swRingBlocks {
			diff = swRingBlocks
		}

		for i := 0; i < diff; i++ {
			lastBlockIndex = (lastBlockIndex + 1) & swBlockMask
			f.ring[lastBlockIndex] = 0
		}

		f.last = counter
	}

	blockIndex &= swBlockMask
	bitIndex := counter & swBitMask
	f.ring[blockIndex] |= 1 << bitIndex
}
