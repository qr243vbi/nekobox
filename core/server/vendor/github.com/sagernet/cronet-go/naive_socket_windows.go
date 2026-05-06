//go:build windows

package cronet

import (
	"net"
	"syscall"

	E "github.com/sagernet/sing/common/exceptions"

	"golang.org/x/sys/windows"
)

func dupSocketFD(syscallConn syscall.Conn) (int, error) {
	rawConn, err := syscallConn.SyscallConn()
	if err != nil {
		return -1, E.Cause(err, "get syscall conn")
	}
	var socket windows.Handle
	var controlError error
	err = rawConn.Control(func(fdPtr uintptr) {
		currentProcess := windows.CurrentProcess()
		duplicateError := windows.DuplicateHandle(
			currentProcess,
			windows.Handle(fdPtr),
			currentProcess,
			&socket,
			0,
			false,
			windows.DUPLICATE_SAME_ACCESS,
		)
		if duplicateError != nil {
			controlError = E.Cause(duplicateError, "duplicate socket handle")
			return
		}
	})
	if err != nil {
		if socket != 0 {
			_ = windows.Closesocket(socket)
		}
		return -1, E.Cause(err, "control raw conn")
	}
	if controlError != nil {
		return -1, controlError
	}
	return int(socket), nil
}

func createTCPLoopbackSocketPair() (cronetFD int, proxyConn net.Conn, err error) {
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		return -1, nil, E.Cause(err, "create loopback listener")
	}
	defer listener.Close()

	var clientConn net.Conn
	var clientError error
	done := make(chan struct{})
	go func() {
		clientConn, clientError = net.Dial("tcp", listener.Addr().String())
		close(done)
	}()

	serverConn, err := listener.Accept()
	if err != nil {
		<-done
		if clientConn != nil {
			clientConn.Close()
		}
		return -1, nil, E.Cause(err, "accept loopback connection")
	}

	<-done
	if clientError != nil {
		serverConn.Close()
		return -1, nil, E.Cause(clientError, "dial loopback")
	}

	tcpConn := serverConn.(*net.TCPConn)
	fd, err := dupSocketFD(tcpConn)
	if err != nil {
		serverConn.Close()
		clientConn.Close()
		return -1, nil, E.Cause(err, "dup loopback socket")
	}

	serverConn.Close()
	return fd, clientConn, nil
}
