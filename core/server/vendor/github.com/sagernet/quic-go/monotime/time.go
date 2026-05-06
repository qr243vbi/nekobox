package monotime

import (
	"time"

	"github.com/sagernet/quic-go/internal/monotime"
)

// A Time represents an instant in monotonic time.
// Times can be compared using the comparison operators, but the specific
// value is implementation-dependent and should not be relied upon.
// The zero value of Time doesn't have any specific meaning.
type Time = monotime.Time

// Now returns the current monotonic time.
func Now() Time {
	return monotime.Now()
}

// Since returns the time elapsed since t. It is shorthand for Now().Sub(t).
func Since(t Time) time.Duration {
	return monotime.Since(t)
}

// Until returns the duration until t.
// It is shorthand for t.Sub(Now()).
// If t is in the past, the returned duration will be negative.
func Until(t Time) time.Duration {
	return monotime.Until(t)
}

// FromTime converts a time.Time to a monotonic Time.
// The conversion is relative to the package's start time and may lose
// precision if the time.Time is far from the start time.
func FromTime(t time.Time) Time {
	return monotime.FromTime(t)
}
