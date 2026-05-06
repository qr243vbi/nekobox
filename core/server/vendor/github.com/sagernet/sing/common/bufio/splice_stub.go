//go:build !linux

package bufio

import (
	"syscall"

	N "github.com/sagernet/sing/common/network"
)

func splice(source syscall.RawConn, sourceReader N.SyscallReader, destination syscall.RawConn, destinationWriter N.SyscallWriter, readCounters []N.CountFunc, writeCounters []N.CountFunc) (handed bool, n int64, err error) {
	return
}
