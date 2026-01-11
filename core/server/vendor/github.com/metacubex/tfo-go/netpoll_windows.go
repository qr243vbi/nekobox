package tfo

import (
	"net"
	"strconv"
	"syscall"
	"time"
	"unsafe"
)

// Copied from src/net/tcpsock_posix.go
func sockaddrToTCP(sa syscall.Sockaddr) net.Addr {
	switch sa := sa.(type) {
	case *syscall.SockaddrInet4:
		return &net.TCPAddr{IP: sa.Addr[0:], Port: sa.Port}
	case *syscall.SockaddrInet6:
		zone := ""
		if sa.ZoneId != 0 {
			zone = strconv.Itoa(int(sa.ZoneId))
		}
		return &net.TCPAddr{IP: sa.Addr[0:], Port: sa.Port, Zone: zone}
	}
	return nil
}

func (fd *pFD) ConnectEx(ra syscall.Sockaddr, b []byte) (n int, err error) {
	fd.wop.sa = ra
	n, err = execIO(&fd.wop, func(o *operation) error {
		return syscall.ConnectEx(o.fd.Sysfd, o.sa, &b[0], uint32(len(b)), &o.qty, &o.o)
	})
	return
}

// Network file descriptor.
//
// Copied from src/net/fd_posix.go
type netFD struct {
	pfd pFD

	// immutable until Close
	family      int
	sotype      int
	isConnected bool // handshake completed or use of association with peer
	net         string
	laddr       net.Addr
	raddr       net.Addr
}

func newFD(sysfd syscall.Handle, family, sotype int, net string) (*netFD, error) {
	ret := &netFD{
		pfd: pFD{
			Sysfd:         sysfd,
			IsStream:      sotype == syscall.SOCK_STREAM,
			ZeroReadIsEOF: sotype != syscall.SOCK_DGRAM && sotype != syscall.SOCK_RAW,
		},
		family: family,
		sotype: sotype,
		net:    net,
	}
	return ret, nil
}

func netFDClose(fd *netFD) error {
	return (*net.TCPConn)(unsafe.Pointer(&fd)).Close()
}

func (fd *netFD) ctrlNetwork() string {
	switch fd.net {
	case "unix", "unixgram", "unixpacket":
		return fd.net
	}
	switch fd.net[len(fd.net)-1] {
	case '4', '6':
		return fd.net
	}
	if fd.family == syscall.AF_INET {
		return fd.net + "4"
	}
	return fd.net + "6"
}

func (fd *netFD) Close() error {
	return (*net.TCPConn)(unsafe.Pointer(&fd)).Close()
}

func (fd *netFD) Write(p []byte) (int, error) {
	return (*net.TCPConn)(unsafe.Pointer(&fd)).Write(p)
}

func (fd *netFD) SetWriteDeadline(t time.Time) error {
	return (*net.TCPConn)(unsafe.Pointer(&fd)).SetWriteDeadline(t)
}

func (fd *netFD) SyscallConn() (syscall.RawConn, error) {
	return (*net.TCPConn)(unsafe.Pointer(&fd)).SyscallConn()
}
