//go:build unix

package control

import (
	"syscall"

	E "github.com/sagernet/sing/common/exceptions"
)

func sendAncillaryFileDescriptors(protectPath string, fileDescriptors []int) error {
	socket, err := syscall.Socket(syscall.AF_UNIX, syscall.SOCK_STREAM, 0)
	if err != nil {
		return E.Cause(err, "open protect socket")
	}
	defer syscall.Close(socket)
	err = syscall.Connect(socket, &syscall.SockaddrUnix{Name: protectPath})
	if err != nil {
		return E.Cause(err, "connect protect path")
	}
	oob := syscall.UnixRights(fileDescriptors...)
	dummy := []byte{1}
	err = syscall.Sendmsg(socket, dummy, oob, nil, 0)
	if err != nil {
		return err
	}
	n, err := syscall.Read(socket, dummy)
	if err != nil {
		return err
	}
	if n != 1 {
		return E.New("failed to protect fd")
	}
	return nil
}

func ProtectPath(protectPath string) Func {
	return func(network, address string, conn syscall.RawConn) error {
		return Raw(conn, func(fd uintptr) error {
			return sendAncillaryFileDescriptors(protectPath, []int{int(fd)})
		})
	}
}
