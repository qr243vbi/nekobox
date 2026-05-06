package bufio

import (
	"net/netip"
	"sync"

	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/control"
	M "github.com/sagernet/sing/common/metadata"

	"golang.org/x/sys/windows"
)

type syscallVectorisedWriterFields struct {
	access    sync.Mutex
	iovecList []windows.WSABuf
	localAddr netip.AddrPort
}

func (w *SyscallVectorisedWriter) WriteVectorised(buffers []*buf.Buffer) error {
	/*w.access.Lock()
	defer w.access.Unlock()
	defer buf.ReleaseMulti(buffers)
	iovecList := w.iovecList
	for _, buffer := range buffers {
		iovecList = append(iovecList, buffer.Iovec(buffer.Len()))
	}
	var n uint32
	var innerErr error
	err := w.rawConn.Write(func(fd uintptr) (done bool) {
		innerErr = windows.WSASend(windows.Handle(fd), &iovecList[0], uint32(len(iovecList)), &n, 0, nil, nil)
		return innerErr != windows.WSAEWOULDBLOCK
	})
	common.ClearArray(iovecList)
	if cap(iovecList) > cap(w.iovecList) {
		w.iovecList = w.iovecList[:0]
	}
	if innerErr != nil {
		err = innerErr
	}
	return err*/
	panic("not implemented")
}

func (w *SyscallVectorisedPacketWriter) WriteVectorisedPacket(buffers []*buf.Buffer, destination M.Socksaddr) error {
	w.access.Lock()
	defer w.access.Unlock()
	if !w.localAddr.IsValid() {
		err := control.Raw(w.rawConn, func(fd uintptr) error {
			name, err := windows.Getsockname(windows.Handle(fd))
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
	var n uint32
	var innerErr error
	err := w.rawConn.Write(func(fd uintptr) (done bool) {
		name, nameLen := M.AddrPortToRawSockaddr(destination.AddrPort(), w.localAddr.Addr().Is6())
		var bufs *windows.WSABuf
		if len(iovecList) > 0 {
			bufs = &iovecList[0]
		}
		innerErr = windows.WSASendTo(
			windows.Handle(fd),
			bufs,
			uint32(len(iovecList)),
			&n,
			0,
			(*windows.RawSockaddrAny)(name),
			nameLen,
			nil,
			nil)
		return innerErr != windows.WSAEWOULDBLOCK
	})
	for i := range iovecList {
		iovecList[i] = windows.WSABuf{}
	}
	if cap(iovecList) > cap(w.iovecList) {
		w.iovecList = w.iovecList[:0]
	}
	if innerErr != nil {
		err = innerErr
	}
	return err
}
