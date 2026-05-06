//go:build unix

package cronet

import (
	"net"
	"os"
	"syscall"

	E "github.com/sagernet/sing/common/exceptions"
)

func dupSocketFD(syscallConn syscall.Conn) (int, error) {
	rawConn, err := syscallConn.SyscallConn()
	if err != nil {
		return -1, E.Cause(err, "get syscall conn")
	}
	var fd int
	var controlError error
	err = rawConn.Control(func(fdPtr uintptr) {
		newFD, dupError := syscall.Dup(int(fdPtr))
		if dupError != nil {
			controlError = E.Cause(dupError, "dup socket fd")
			return
		}
		syscall.CloseOnExec(newFD)
		fd = newFD
	})
	if err != nil {
		return -1, E.Cause(err, "control raw conn")
	}
	if controlError != nil {
		return -1, controlError
	}
	return fd, nil
}

func createSocketPair() (cronetFD int, proxyConn net.Conn, err error) {
	fds, err := syscall.Socketpair(syscall.AF_UNIX, syscall.SOCK_STREAM, 0)
	if err != nil {
		return -1, nil, E.Cause(err, "create socketpair")
	}

	syscall.CloseOnExec(fds[0])

	file := os.NewFile(uintptr(fds[1]), "cronet-socketpair")
	conn, err := net.FileConn(file)
	_ = file.Close()
	if err != nil {
		syscall.Close(fds[0])
		return -1, nil, E.Cause(err, "create net conn from socketpair")
	}
	return fds[0], conn, nil
}
