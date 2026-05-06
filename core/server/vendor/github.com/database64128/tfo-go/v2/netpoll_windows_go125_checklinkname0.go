//go:build windows && go1.25 && !go1.26 && tfogo_checklinkname0

package tfo

import (
	"sync"
	"sync/atomic"

	"golang.org/x/sys/windows"
)

// pFD is a file descriptor. The net and os packages embed this type in
// a larger type representing a network connection or OS file.
//
// Stay in sync with FD in src/internal/poll/fd_windows.go
type pFD struct {
	fdmuS uint64
	fdmuR uint32
	fdmuW uint32

	// System file descriptor. Immutable until Close.
	Sysfd windows.Handle

	// Read operation.
	rop operation
	// Write operation.
	wop operation

	// I/O poller.
	pd uintptr

	// Used to implement pread/pwrite.
	l sync.Mutex

	// The file offset for the next read or write.
	// Overlapped IO operations don't use the real file pointer,
	// so we need to keep track of the offset ourselves.
	offset int64

	// For console I/O.
	lastbits       []byte   // first few bytes of the last incomplete rune in last write
	readuint16     []uint16 // buffer to hold uint16s obtained with ReadConsole
	readbyte       []byte   // buffer to hold decoding of readuint16 from utf16 to utf8
	readbyteOffset int      // readbyte[readOffset:] is yet to be consumed with file.Read

	// Semaphore signaled when file is closed.
	csema uint32

	skipSyncNotif bool

	// Whether this is a streaming descriptor, as opposed to a
	// packet-based descriptor like a UDP socket.
	IsStream bool

	// Whether a zero byte read indicates EOF. This is false for a
	// message based socket connection.
	ZeroReadIsEOF bool

	// Whether the handle is owned by os.File.
	isFile bool

	// The kind of this file.
	kind byte

	// Whether FILE_FLAG_OVERLAPPED was not set when opening the file.
	isBlocking bool

	disassociated atomic.Bool
}
