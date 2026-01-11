package mux

import (
	"net"
	"os"
	"reflect"
	"syscall"
	"unsafe"
	_ "unsafe"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/control"
	E "github.com/sagernet/sing/common/exceptions"

	"golang.org/x/sys/unix"
)

const (
	BrutalAvailable   = true
	TCP_BRUTAL_PARAMS = 23301
)

type TCPBrutalParams struct {
	Rate     uint64
	CwndGain uint32
}

//go:linkname setsockopt syscall.setsockopt
func setsockopt(s int, level int, name int, val unsafe.Pointer, vallen uintptr) (err error)

func SetBrutalOptions(conn net.Conn, sendBPS uint64) error {
	syscallConn, loaded := common.Cast[syscall.Conn](conn)
	if !loaded {
		return E.New(
			"brutal: nested multiplexing is not supported: ",
			"cannot convert ", reflect.TypeOf(conn), " to syscall.Conn, final type: ", reflect.TypeOf(common.Top(conn)),
		)
	}
	return control.Conn(syscallConn, func(fd uintptr) error {
		err := unix.SetsockoptString(int(fd), unix.IPPROTO_TCP, unix.TCP_CONGESTION, "brutal")
		if err != nil {
			return E.Extend(
				os.NewSyscallError("setsockopt IPPROTO_TCP TCP_CONGESTION brutal", err),
				"please make sure you have installed the tcp-brutal kernel module",
			)
		}
		params := TCPBrutalParams{
			Rate:     sendBPS,
			CwndGain: 20, // hysteria2 default
		}
		err = setsockopt(int(fd), unix.IPPROTO_TCP, TCP_BRUTAL_PARAMS, unsafe.Pointer(&params), unsafe.Sizeof(params))
		if err != nil {
			return os.NewSyscallError("setsockopt IPPROTO_TCP TCP_BRUTAL_PARAMS", err)
		}
		return nil
	})
}
