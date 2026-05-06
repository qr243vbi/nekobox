package logger

import (
	"context"
)

func NOP() ContextLogger {
	return (*nopLogger)(nil)
}

type nopLogger struct{}

func (f *nopLogger) Trace(args ...any) {
}

func (f *nopLogger) Debug(args ...any) {
}

func (f *nopLogger) Info(args ...any) {
}

func (f *nopLogger) Warn(args ...any) {
}

func (f *nopLogger) Error(args ...any) {
}

func (f *nopLogger) Fatal(args ...any) {
}

func (f *nopLogger) Panic(args ...any) {
}

func (f *nopLogger) TraceContext(ctx context.Context, args ...any) {
}

func (f *nopLogger) DebugContext(ctx context.Context, args ...any) {
}

func (f *nopLogger) InfoContext(ctx context.Context, args ...any) {
}

func (f *nopLogger) WarnContext(ctx context.Context, args ...any) {
}

func (f *nopLogger) ErrorContext(ctx context.Context, args ...any) {
}

func (f *nopLogger) FatalContext(ctx context.Context, args ...any) {
}

func (f *nopLogger) PanicContext(ctx context.Context, args ...any) {
}
