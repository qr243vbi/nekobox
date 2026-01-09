//go:build !windows

package bufio

import (
	"net/netip"
	"os"
	"sync"
	"unsafe"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/control"
	M "github.com/sagernet/sing/common/metadata"

	"golang.org/x/sys/unix"
)

type syscallVectorisedWriterFields struct {
	access    sync.Mutex
	iovecList []unix.Iovec
	localAddr netip.AddrPort
}

func (w *SyscallVectorisedWriter) WriteVectorised(buffers []*buf.Buffer) error {
	w.access.Lock()
	defer w.access.Unlock()
	defer buf.ReleaseMulti(buffers)
	iovecList := w.iovecList
	for _, buffer := range buffers {
		if buffer.IsEmpty() {
			continue
		}
		iovecList = append(iovecList, buffer.Iovec(buffer.Len()))
	}
	if len(iovecList) == 0 {
		return os.ErrInvalid
	} else if cap(iovecList) > cap(w.iovecList) {
		w.iovecList = iovecList[:0]
	}
	var innerErr unix.Errno
	writeIovecList := iovecList
	err := w.rawConn.Write(func(fd uintptr) (done bool) {
		for {
			var r0 uintptr
			//nolint:staticcheck
			r0, _, innerErr = unix.RawSyscall(unix.SYS_WRITEV, fd, uintptr(unsafe.Pointer(&writeIovecList[0])), uintptr(len(writeIovecList)))
			writeN := int(r0)
			for writeN > 0 && len(writeIovecList) > 0 {
				if int(writeIovecList[0].Len) > writeN {
					writeIovecList[0].Base = (*byte)(unsafe.Add(unsafe.Pointer(writeIovecList[0].Base), writeN))
					writeIovecList[0].SetLen(int(writeIovecList[0].Len) - writeN)
					break
				} else {
					writeN -= int(writeIovecList[0].Len)
					writeIovecList = writeIovecList[1:]
				}
			}
			if innerErr == unix.EINTR || (innerErr == 0 && len(writeIovecList) > 0) {
				continue
			}
			return innerErr != unix.EAGAIN
		}
	})
	common.ClearArray(iovecList)
	if innerErr != 0 {
		err = os.NewSyscallError("SYS_WRITEV", innerErr)
	}
	return err
}

func (w *SyscallVectorisedPacketWriter) WriteVectorisedPacket(buffers []*buf.Buffer, destination M.Socksaddr) error {
	w.access.Lock()
	defer w.access.Unlock()
	if !w.localAddr.IsValid() {
		err := control.Raw(w.rawConn, func(fd uintptr) error {
			name, err := unix.Getsockname(int(fd))
			if err != nil {
				return err
			}
			w.localAddr = M.AddrPortFromSockaddr(name)
			return nil
		})
		if err != nil {
			return err
		}
	}
	defer buf.ReleaseMulti(buffers)
	iovecList := w.iovecList
	for _, buffer := range buffers {
		if buffer.IsEmpty() {
			continue
		}
		iovecList = append(iovecList, buffer.Iovec(buffer.Len()))
	}
	if cap(iovecList) > cap(w.iovecList) {
		w.iovecList = iovecList[:0]
	}
	var innerErr unix.Errno
	err := w.rawConn.Write(func(fd uintptr) (done bool) {
		var msg unix.Msghdr
		name, nameLen := M.AddrPortToRawSockaddr(destination.AddrPort(), w.localAddr.Addr().Is6())
		msg.Name = (*byte)(name)
		msg.Namelen = nameLen
		if len(iovecList) > 0 {
			msg.Iov = &iovecList[0]
			msg.SetIovlen(len(iovecList))
		}
		for {
			//nolint:staticcheck
			_, _, innerErr = unix.RawSyscall(unix.SYS_SENDMSG, fd, uintptr(unsafe.Pointer(&msg)), 0)
			if innerErr == unix.EINTR {
				continue
			}
			return innerErr != unix.EAGAIN
		}
	})
	common.ClearArray(iovecList)
	if innerErr != 0 {
		err = os.NewSyscallError("SYS_SENDMSG", innerErr)
	}
	return err
}
