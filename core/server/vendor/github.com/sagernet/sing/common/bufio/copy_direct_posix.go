//go:build !windows

package bufio

import (
	"io"
	"net/netip"
	"os"
	"syscall"
	"unsafe"

	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"

	"golang.org/x/sys/unix"
)

var _ N.ReadWaiter = (*syscallReadWaiter)(nil)

type syscallReadWaiter struct {
	rawConn  syscall.RawConn
	readErr  error
	readFunc func(fd uintptr) (done bool)
	buffer   *buf.Buffer
	options  N.ReadWaitOptions
}

func createSyscallReadWaiter(reader any) (*syscallReadWaiter, bool) {
	if syscallConn, isSyscallConn := reader.(syscall.Conn); isSyscallConn {
		rawConn, err := syscallConn.SyscallConn()
		if err == nil {
			return &syscallReadWaiter{rawConn: rawConn}, true
		}
	}
	return nil, false
}

func (w *syscallReadWaiter) InitializeReadWaiter(options N.ReadWaitOptions) (needCopy bool) {
	w.options = options
	w.readFunc = func(fd uintptr) (done bool) {
		buffer := w.options.NewBuffer()
		var readN int
		readN, w.readErr = unix.Read(int(fd), buffer.FreeBytes())
		if readN > 0 {
			buffer.Truncate(readN)
			w.options.PostReturn(buffer)
			w.buffer = buffer
		} else {
			buffer.Release()
		}
		//goland:noinspection GoDirectComparisonOfErrors
		if w.readErr == unix.EAGAIN {
			return false
		}
		if readN == 0 && w.readErr == nil {
			w.readErr = io.EOF
		}
		return true
	}
	return false
}

func (w *syscallReadWaiter) WaitReadBuffer() (buffer *buf.Buffer, err error) {
	if w.readFunc == nil {
		return nil, os.ErrInvalid
	}
	err = w.rawConn.Read(w.readFunc)
	if err != nil {
		return
	}
	if w.readErr != nil {
		if w.readErr == io.EOF {
			return nil, io.EOF
		}
		return nil, E.Cause(w.readErr, "raw read")
	}
	buffer = w.buffer
	w.buffer = nil
	return
}

var _ N.VectorisedReadWaiter = (*vectorisedSyscallReadWaiter)(nil)

type vectorisedSyscallReadWaiter struct {
	rawConn     syscall.RawConn
	readBuffers int
	readErr     error
	readFunc    func(fd uintptr) (done bool)
	buffers     []*buf.Buffer
	iovecList   []unix.Iovec
	options     N.ReadWaitOptions
}

func createVectorisedSyscallReadWaiter(reader any) (*vectorisedSyscallReadWaiter, bool) {
	if syscallConn, isSyscallConn := reader.(syscall.Conn); isSyscallConn {
		rawConn, err := syscallConn.SyscallConn()
		if err == nil {
			return &vectorisedSyscallReadWaiter{rawConn: rawConn}, true
		}
	}
	return nil, false
}

func (w *vectorisedSyscallReadWaiter) InitializeReadWaiter(options N.ReadWaitOptions) (needCopy bool) {
	w.options = options
	w.buffers = make([]*buf.Buffer, options.BatchSize)
	w.iovecList = make([]unix.Iovec, options.BatchSize)
	w.readFunc = func(fd uintptr) (done bool) {
		for i := range w.buffers {
			buffer := w.buffers[i]
			if buffer == nil {
				buffer = w.options.NewBuffer()
				w.buffers[i] = buffer
			}
			w.iovecList[i] = buffer.Iovec(buffer.Cap())
		}
		var (
			readN     uintptr
			readErrno unix.Errno
		)
		//nolint:staticcheck
		readN, _, readErrno = unix.Syscall(unix.SYS_READV, fd, uintptr(unsafe.Pointer(&w.iovecList[0])), uintptr(len(w.iovecList)))
		//goland:noinspection GoDirectComparisonOfErrors
		if readErrno == unix.EAGAIN || readErrno == unix.EWOULDBLOCK {
			return false
		} else if readErrno != 0 {
			w.readErr = readErrno
		} else {
			w.readErr = nil
		}
		if readN > 0 {
			pendingN := int(readN)
			for _, buffer := range w.buffers {
				w.readBuffers++
				if pendingN > buffer.FreeLen() {
					pendingN -= buffer.FreeLen()
					buffer.Truncate(buffer.FreeLen())
					w.options.PostReturn(buffer)
				} else {
					buffer.Truncate(pendingN)
					w.options.PostReturn(buffer)
					break
				}
			}
		}
		if readN == 0 && w.readErr == nil {
			w.readErr = io.EOF
		}
		return true
	}
	return false
}

func (w *vectorisedSyscallReadWaiter) WaitReadBuffers() (buffers []*buf.Buffer, err error) {
	if w.readFunc == nil {
		return nil, os.ErrInvalid
	}
	err = w.rawConn.Read(w.readFunc)
	if err != nil {
		return
	}
	if w.readErr != nil {
		if w.readErr == io.EOF {
			return nil, io.EOF
		}
		return nil, E.Cause(w.readErr, "raw read")
	}
	buffers = make([]*buf.Buffer, w.readBuffers)
	for i := 0; i < w.readBuffers; i++ {
		buffers[i] = w.buffers[i]
		w.buffers[i] = nil
	}
	w.readBuffers = 0
	return
}

var _ N.PacketReadWaiter = (*syscallPacketReadWaiter)(nil)

type syscallPacketReadWaiter struct {
	rawConn  syscall.RawConn
	readErr  error
	readFrom M.Socksaddr
	readFunc func(fd uintptr) (done bool)
	buffer   *buf.Buffer
	options  N.ReadWaitOptions
}

func createSyscallPacketReadWaiter(reader any) (*syscallPacketReadWaiter, bool) {
	if syscallConn, isSyscallConn := reader.(syscall.Conn); isSyscallConn {
		rawConn, err := syscallConn.SyscallConn()
		if err == nil {
			return &syscallPacketReadWaiter{rawConn: rawConn}, true
		}
	}
	return nil, false
}

func (w *syscallPacketReadWaiter) InitializeReadWaiter(options N.ReadWaitOptions) (needCopy bool) {
	w.options = options
	w.readFunc = func(fd uintptr) (done bool) {
		buffer := w.options.NewPacketBuffer()
		var readN int
		var from unix.Sockaddr
		readN, _, _, from, w.readErr = unix.Recvmsg(int(fd), buffer.FreeBytes(), nil, 0)
		//goland:noinspection GoDirectComparisonOfErrors
		if w.readErr != nil {
			buffer.Release()
			return w.readErr != syscall.EAGAIN
		}
		if readN > 0 {
			buffer.Truncate(readN)
		}
		w.options.PostReturn(buffer)
		w.buffer = buffer
		switch fromAddr := from.(type) {
		case *unix.SockaddrInet4:
			w.readFrom = M.SocksaddrFrom(netip.AddrFrom4(fromAddr.Addr), uint16(fromAddr.Port))
		case *unix.SockaddrInet6:
			w.readFrom = M.SocksaddrFrom(netip.AddrFrom16(fromAddr.Addr), uint16(fromAddr.Port)).Unwrap()
		}
		return true
	}
	return false
}

func (w *syscallPacketReadWaiter) WaitReadPacket() (buffer *buf.Buffer, destination M.Socksaddr, err error) {
	if w.readFunc == nil {
		return nil, M.Socksaddr{}, os.ErrInvalid
	}
	err = w.rawConn.Read(w.readFunc)
	if err != nil {
		return
	}
	if w.readErr != nil {
		err = E.Cause(w.readErr, "raw read")
		return
	}
	buffer = w.buffer
	w.buffer = nil
	destination = w.readFrom
	return
}
