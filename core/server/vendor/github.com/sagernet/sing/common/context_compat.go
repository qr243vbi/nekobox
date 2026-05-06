//go:build go1.20

package common

import "context"

type (
	ContextCancelCauseFunc = context.CancelCauseFunc
)

var (
	ContextWithCancelCause = context.WithCancelCause
	ContextCause           = context.Cause
)
