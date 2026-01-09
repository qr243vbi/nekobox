package bufio

import (
	"syscall"

	E "github.com/sagernet/sing/common/exceptions"
	N "github.com/sagernet/sing/common/network"

	"golang.org/x/sys/unix"
)

const maxSpliceSize = 1 << 20

func splice(source syscall.RawConn, destination syscall.RawConn, readCounters []N.CountFunc, writeCounters []N.CountFunc) (handed bool, n int64, err error) {
	handed = true
	var pipeFDs [2]int
	err = unix.Pipe2(pipeFDs[:], syscall.O_CLOEXEC|syscall.O_NONBLOCK)
	if err != nil {
		return
	}
	defer unix.Close(pipeFDs[0])
	defer unix.Close(pipeFDs[1])

	_, _ = unix.FcntlInt(uintptr(pipeFDs[0]), unix.F_SETPIPE_SZ, maxSpliceSize)
	var readN int
	var readErr error
	var writeSize int
	var writeErr error
	readFunc := func(fd uintptr) (done bool) {
		p0, p1 := unix.Splice(int(fd), nil, pipeFDs[1], nil, maxSpliceSize, unix.SPLICE_F_NONBLOCK)
		readN = int(p0)
		readErr = p1
		return readErr != unix.EAGAIN
	}
	writeFunc := func(fd uintptr) (done bool) {
		for writeSize > 0 {
			p0, p1 := unix.Splice(pipeFDs[0], nil, int(fd), nil, writeSize, unix.SPLICE_F_NONBLOCK|unix.SPLICE_F_MOVE)
			writeN := int(p0)
			writeErr = p1
			if writeErr != nil {
				return writeErr != unix.EAGAIN
			}
			writeSize -= writeN
		}
		return true
	}
	for {
		err = source.Read(readFunc)
		if err != nil {
			readErr = err
		}
		if readErr != nil {
			if readErr == unix.EINVAL || readErr == unix.ENOSYS {
				handed = false
				return
			}
			err = E.Cause(readErr, "splice read")
			return
		}
		if readN == 0 {
			return
		}
		writeSize = readN
		err = destination.Write(writeFunc)
		if err != nil {
			writeErr = err
		}
		if writeErr != nil {
			err = E.Cause(writeErr, "splice write")
			return
		}
		for _, readCounter := range readCounters {
			readCounter(int64(readN))
		}
		for _, writeCounter := range writeCounters {
			writeCounter(int64(readN))
		}
	}
}
