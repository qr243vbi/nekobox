package atomic

import "sync/atomic"

type (
	// Deprecated: use sync/atomic instead
	Bool = atomic.Bool
	// Deprecated: use sync/atomic instead
	Int32 = atomic.Int32
	// Deprecated: use sync/atomic instead
	Int64 = atomic.Int64
	// Deprecated: use sync/atomic instead
	Uint32 = atomic.Uint32
	// Deprecated: use sync/atomic instead
	Uint64 = atomic.Uint64
	// Deprecated: use sync/atomic instead
	Uintptr = atomic.Uintptr
	// Deprecated: use sync/atomic instead
	Value = atomic.Value
)

// Deprecated: use sync/atomic instead
type Pointer[T any] struct {
	atomic.Pointer[T]
}
