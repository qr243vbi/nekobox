//go:build windows && go1.25

package tfo

import (
	"sync"
	"sync/atomic"
	"syscall"
	"unsafe"
)

// pFD is a file descriptor. The net and os packages embed this type in
// a larger type representing a network connection or OS file.
//
// Stay in sync with FD in src/internal/poll/fd_windows.go
type pFD struct {
	// Lock sysfd and serialize access to Read and Write methods.
	fdmu fdMutex

	// System file descriptor. Immutable until Close.
	Sysfd syscall.Handle

	// Read operation.
	rop operation
	// Write operation.
	wop operation

	// I/O poller.
	pd pollDesc

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

	// Whether this is a file rather than a network socket.
	isFile bool

	// The kind of this file.
	kind fileKind

	// Whether FILE_FLAG_OVERLAPPED was not set when opening the file.
	isBlocking bool

	disassociated atomic.Bool
}

// Copied from internal/poll/fd_mutex.go

// fdMutex.state is organized as follows:
// 1 bit - whether FD is closed, if set all subsequent lock operations will fail.
// 1 bit - lock for read operations.
// 1 bit - lock for write operations.
// 20 bits - total number of references (read+write+misc).
// 20 bits - number of outstanding read waiters.
// 20 bits - number of outstanding write waiters.
const (
	mutexClosed  = 1 << 0
	mutexRLock   = 1 << 1
	mutexWLock   = 1 << 2
	mutexRef     = 1 << 3
	mutexRefMask = (1<<20 - 1) << 3
	mutexRWait   = 1 << 23
	mutexRMask   = (1<<20 - 1) << 23
	mutexWWait   = 1 << 43
	mutexWMask   = (1<<20 - 1) << 43
)

// fdMutex is a specialized synchronization primitive that manages
// lifetime of an fd and serializes access to Read, Write and Close
// methods on FD.
type fdMutex struct {
	state uint64
	rsema uint32
	wsema uint32
}

func (fd *netFD) init() error {
	if err := fd.pfd.Init(fd.net, true); err != nil {
		return err
	}
	switch fd.net {
	case "udp", "udp4", "udp6":
		// Disable reporting of PORT_UNREACHABLE errors.
		// See https://go.dev/issue/5834.
		ret := uint32(0)
		flag := uint32(0)
		size := uint32(unsafe.Sizeof(flag))
		err := syscall.WSAIoctl(fd.pfd.Sysfd, syscall.SIO_UDP_CONNRESET, (*byte)(unsafe.Pointer(&flag)), size, nil, 0, &ret, nil, 0)
		if err != nil {
			return wrapSyscallError("wsaioctl", err)
		}
		// Disable reporting of NET_UNREACHABLE errors.
		// See https://go.dev/issue/68614.
		ret = 0
		flag = 0
		size = uint32(unsafe.Sizeof(flag))
		const SIO_UDP_NETRESET = syscall.IOC_IN | syscall.IOC_VENDOR | 15
		err = syscall.WSAIoctl(fd.pfd.Sysfd, SIO_UDP_NETRESET, (*byte)(unsafe.Pointer(&flag)), size, nil, 0, &ret, nil, 0)
		if err != nil {
			return wrapSyscallError("wsaioctl", err)
		}
	}
	return nil
}

// pollable should be used instead of fd.pd.pollable(),
// as it is aware of the disassociated state.
func (fd *pFD) pollable() bool {
	return fd.pd.pollable() && !fd.disassociated.Load()
}

// closing returns true if fd is closing.
func (fd *pFD) closing() bool {
	return atomic.LoadUint64(&fd.fdmu.state)&mutexClosed != 0
}
