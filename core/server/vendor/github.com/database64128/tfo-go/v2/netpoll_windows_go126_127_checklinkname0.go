//go:build windows && go1.26 && tfogo_checklinkname0

package tfo

import "golang.org/x/sys/windows"

// operation contains superset of data necessary to perform all async IO.
//
// Copied from src/internal/poll/fd_windows.go
type operation struct {
	// Used by IOCP interface, it must be first field
	// of the struct, as our code relies on it.
	o windows.Overlapped

	// fields used by runtime.netpoll
	runtimeCtx uintptr
	mode       int32
}
