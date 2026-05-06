//go:build go1.21

package common

import "sync"

// OnceFunc is a wrapper around sync.OnceFunc.
func OnceFunc(f func()) func() {
	return sync.OnceFunc(f)
}

// OnceValue is a wrapper around sync.OnceValue.
func OnceValue[T any](f func() T) func() T {
	return sync.OnceValue(f)
}

// OnceValues is a wrapper around sync.OnceValues.
func OnceValues[T1, T2 any](f func() (T1, T2)) func() (T1, T2) {
	return sync.OnceValues(f)
}
