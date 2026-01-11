package tfo

import (
	"syscall"
	"unsafe"

	"golang.org/x/sys/unix"
)

// Do the interface allocations only once for common
// Errno values.
var (
	errEAGAIN error = syscall.EAGAIN
	errEINVAL error = syscall.EINVAL
	errENOENT error = syscall.ENOENT
)

// errnoErr returns common boxed Errno values, to prevent
// allocations at runtime.
func errnoErr(e syscall.Errno) error {
	switch e {
	case 0:
		return nil
	case unix.EAGAIN:
		return errEAGAIN
	case unix.EINVAL:
		return errEINVAL
	case unix.ENOENT:
		return errENOENT
	}
	return e
}

func sockaddrp(sa syscall.Sockaddr) (unsafe.Pointer, uint32, error) {
	switch sa := sa.(type) {
	case nil:
		return nil, 0, nil
	case *syscall.SockaddrInet4:
		return (*sockaddrInet4)(unsafe.Pointer(sa)).sockaddr()
	case *syscall.SockaddrInet6:
		return (*sockaddrInet6)(unsafe.Pointer(sa)).sockaddr()
	default:
		return nil, 0, syscall.EAFNOSUPPORT
	}
}

// Copied from src/syscall/syscall_unix.go
type sockaddrInet4 struct {
	Port int
	Addr [4]byte
	raw  syscall.RawSockaddrInet4
}

// Copied from src/syscall/syscall_unix.go
type sockaddrInet6 struct {
	Port   int
	ZoneId uint32
	Addr   [16]byte
	raw    syscall.RawSockaddrInet6
}

func (sa *sockaddrInet4) sockaddr() (unsafe.Pointer, uint32, error) {
	if sa.Port < 0 || sa.Port > 0xFFFF {
		return nil, 0, syscall.EINVAL
	}
	sa.raw.Len = syscall.SizeofSockaddrInet4
	sa.raw.Family = syscall.AF_INET
	p := (*[2]byte)(unsafe.Pointer(&sa.raw.Port))
	p[0] = byte(sa.Port >> 8)
	p[1] = byte(sa.Port)
	sa.raw.Addr = sa.Addr
	return unsafe.Pointer(&sa.raw), uint32(sa.raw.Len), nil
}

func (sa *sockaddrInet6) sockaddr() (unsafe.Pointer, uint32, error) {
	if sa.Port < 0 || sa.Port > 0xFFFF {
		return nil, 0, syscall.EINVAL
	}
	sa.raw.Len = syscall.SizeofSockaddrInet6
	sa.raw.Family = syscall.AF_INET6
	p := (*[2]byte)(unsafe.Pointer(&sa.raw.Port))
	p[0] = byte(sa.Port >> 8)
	p[1] = byte(sa.Port)
	sa.raw.Scope_id = sa.ZoneId
	sa.raw.Addr = sa.Addr
	return unsafe.Pointer(&sa.raw), uint32(sa.raw.Len), nil
}

type sa_endpoints_t struct {
	sae_srcif      uint
	sae_srcaddr    unsafe.Pointer
	sae_srcaddrlen uint32
	sae_dstaddr    unsafe.Pointer
	sae_dstaddrlen uint32
}

const (
	SAE_ASSOCID_ANY              = 0
	CONNECT_RESUME_ON_READ_WRITE = 0x1
	CONNECT_DATA_IDEMPOTENT      = 0x2
	CONNECT_DATA_AUTHENTICATED   = 0x4
)

// Connectx enables TFO if a non-empty buf is passed.
// If an empty buf is passed, TFO is not enabled.
func Connectx(s int, srcif uint, from syscall.Sockaddr, to syscall.Sockaddr, buf []byte) (uint, error) {
	from_ptr, from_n, err := sockaddrp(from)
	if err != nil {
		return 0, err
	}

	to_ptr, to_n, err := sockaddrp(to)
	if err != nil {
		return 0, err
	}

	sae := sa_endpoints_t{
		sae_srcif:      srcif,
		sae_srcaddr:    from_ptr,
		sae_srcaddrlen: from_n,
		sae_dstaddr:    to_ptr,
		sae_dstaddrlen: to_n,
	}

	var (
		flags  uint
		iov    *unix.Iovec
		iovcnt uint
	)

	if len(buf) > 0 {
		flags = CONNECT_DATA_IDEMPOTENT
		iov = &unix.Iovec{
			Base: &buf[0],
			Len:  uint64(len(buf)),
		}
		iovcnt = 1
	}

	var bytesSent uint

	r1, _, e1 := unix.Syscall9(unix.SYS_CONNECTX,
		uintptr(s),
		uintptr(unsafe.Pointer(&sae)),
		SAE_ASSOCID_ANY,
		uintptr(flags),
		uintptr(unsafe.Pointer(iov)),
		uintptr(iovcnt),
		uintptr(unsafe.Pointer(&bytesSent)),
		0,
		0)
	ret := int(r1)
	if ret == -1 {
		err = errnoErr(e1)
	}
	return bytesSent, err
}
