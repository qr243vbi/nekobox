package common

import (
	"sync/atomic"
)

type TypedValue[T any] atomic.Pointer[T]

func (t *TypedValue[T]) Load() T {
	value := (*atomic.Pointer[T])(t).Load()
	if value == nil {
		return DefaultValue[T]()
	}
	return *value
}

func (t *TypedValue[T]) Store(value T) {
	(*atomic.Pointer[T])(t).Store(&value)
}

func (t *TypedValue[T]) Swap(new T) T {
	old := (*atomic.Pointer[T])(t).Swap(&new)
	if old == nil {
		return DefaultValue[T]()
	}
	return *old
}

func (t *TypedValue[T]) CompareAndSwap(old, new T) bool {
	for {
		currentP := (*atomic.Pointer[T])(t).Load()
		currentValue := DefaultValue[T]()
		if currentP != nil {
			currentValue = *currentP
		}
		// Compare old and current via runtime equality check.
		if any(currentValue) != any(old) {
			return false
		}
		if (*atomic.Pointer[T])(t).CompareAndSwap(currentP, &new) {
			return true
		}
	}
}
