//go:build unix

package cronet

import (
	"net"
	"os"
	"syscall"

	E "github.com/sagernet/sing/common/exceptions"
)

func createPacketSocketPair(forceUDPLoopback bool) (cronetFD int, proxyConn net.PacketConn, err error) {
	if forceUDPLoopback {
		return createUDPLoopbackPair()
	}

	fds, err := syscall.Socketpair(syscall.AF_UNIX, syscall.SOCK_DGRAM, 0)
	if err != nil {
		return -1, nil, E.Cause(err, "create dgram socketpair")
	}

	syscall.CloseOnExec(fds[0])

	file := os.NewFile(uintptr(fds[1]), "cronet-dgram-socketpair")
	conn, err := net.FilePacketConn(file)
	_ = file.Close()
	if err != nil {
		syscall.Close(fds[0])
		return -1, nil, E.Cause(err, "create packet conn from socketpair")
	}

	return fds[0], conn, nil
}

func createUDPLoopbackPair() (cronetFD int, proxyConn net.PacketConn, err error) {
	// Create two UDP sockets and connect them to each other.
	// Both sockets must be connected for bidirectional communication.

	// Step 1: Create proxyConn socket (unconnected initially to get a port)
	proxyAddress, err := net.ResolveUDPAddr("udp", "127.0.0.1:0")
	if err != nil {
		return -1, nil, err
	}
	proxyUDPConn, err := net.ListenUDP("udp", proxyAddress)
	if err != nil {
		return -1, nil, err
	}
	proxyLocalAddress := proxyUDPConn.LocalAddr().(*net.UDPAddr)

	// Step 2: Create cronetConn socket connected to proxyConn
	cronetAddress, err := net.ResolveUDPAddr("udp", "127.0.0.1:0")
	if err != nil {
		proxyUDPConn.Close()
		return -1, nil, err
	}
	cronetConn, err := net.DialUDP("udp", cronetAddress, proxyLocalAddress)
	if err != nil {
		proxyUDPConn.Close()
		return -1, nil, err
	}
	cronetLocalAddress := cronetConn.LocalAddr().(*net.UDPAddr)

	// Step 3: Connect proxyConn to cronetConn's address using syscall
	proxyRawConn, err := proxyUDPConn.SyscallConn()
	if err != nil {
		cronetConn.Close()
		proxyUDPConn.Close()
		return -1, nil, err
	}

	var connectError error
	err = proxyRawConn.Control(func(fd uintptr) {
		sockaddr := &syscall.SockaddrInet4{Port: cronetLocalAddress.Port}
		copy(sockaddr.Addr[:], cronetLocalAddress.IP.To4())
		connectError = syscall.Connect(int(fd), sockaddr)
	})
	if err != nil {
		cronetConn.Close()
		proxyUDPConn.Close()
		return -1, nil, err
	}
	if connectError != nil {
		cronetConn.Close()
		proxyUDPConn.Close()
		return -1, nil, connectError
	}

	// Step 4: Duplicate cronetConn's fd for Chromium
	cronetRawConn, err := cronetConn.SyscallConn()
	if err != nil {
		cronetConn.Close()
		proxyUDPConn.Close()
		return -1, nil, err
	}

	var cronetFDValue int
	var duplicateError error
	err = cronetRawConn.Control(func(fd uintptr) {
		dupFD, controlErr := syscall.Dup(int(fd))
		if controlErr != nil {
			duplicateError = controlErr
			return
		}
		syscall.CloseOnExec(dupFD)
		cronetFDValue = dupFD
	})
	if err != nil {
		cronetConn.Close()
		proxyUDPConn.Close()
		return -1, nil, err
	}
	if duplicateError != nil {
		cronetConn.Close()
		proxyUDPConn.Close()
		return -1, nil, duplicateError
	}

	cronetConn.Close()

	return cronetFDValue, proxyUDPConn, nil
}
