//go:build !go1.20

package common

import "context"

type ContextCancelCauseFunc func(cause error)

func ContextWithCancelCause(parentContext context.Context) (context.Context, ContextCancelCauseFunc) {
	ctx, cancel := context.WithCancel(parentContext)
	return ctx, func(_ error) { cancel() }
}

func ContextCause(context context.Context) error {
	return context.Err()
}
