//go:build windows

package cronet

import (
	"encoding/binary"
	"errors"
	"io"
	"net"
	"sync"
	"time"

	"golang.org/x/sys/windows"
)

const maxFramedPacketSize = 65535

type framedPacketConn struct {
	conn       net.Conn
	readMutex  sync.Mutex
	writeMutex sync.Mutex
}

var _ net.PacketConn = (*framedPacketConn)(nil)

func newFramedPacketConn(conn net.Conn) *framedPacketConn {
	return &framedPacketConn{conn: conn}
}

func (c *framedPacketConn) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	c.readMutex.Lock()
	defer c.readMutex.Unlock()

	var lengthBuf [2]byte
	_, err = io.ReadFull(c.conn, lengthBuf[:])
	if err != nil {
		return 0, nil, err
	}
	length := binary.BigEndian.Uint16(lengthBuf[:])

	if int(length) > len(p) {
		return 0, nil, errors.New("buffer too small for packet")
	}

	n, err = io.ReadFull(c.conn, p[:length])
	return n, nil, err
}

func (c *framedPacketConn) WriteTo(p []byte, addr net.Addr) (n int, err error) {
	if len(p) > maxFramedPacketSize {
		return 0, errors.New("packet too large")
	}

	// Write length prefix and payload as separate writes to avoid
	// Windows AF_UNIX SOCK_STREAM message-mode behavior where a single
	// large write becomes an indivisible "message" that cannot be read
	// in smaller chunks (WSARecv returns WSAEMSGSIZE).
	c.writeMutex.Lock()
	defer c.writeMutex.Unlock()

	var lengthBuf [2]byte
	binary.BigEndian.PutUint16(lengthBuf[:], uint16(len(p)))
	_, err = c.conn.Write(lengthBuf[:])
	if err != nil {
		return 0, err
	}
	_, err = c.conn.Write(p)
	if err != nil {
		return 0, err
	}
	return len(p), nil
}

func (c *framedPacketConn) Close() error {
	return c.conn.Close()
}

func (c *framedPacketConn) LocalAddr() net.Addr {
	return c.conn.LocalAddr()
}

func (c *framedPacketConn) SetDeadline(t time.Time) error {
	return c.conn.SetDeadline(t)
}

func (c *framedPacketConn) SetReadDeadline(t time.Time) error {
	return c.conn.SetReadDeadline(t)
}

func (c *framedPacketConn) SetWriteDeadline(t time.Time) error {
	return c.conn.SetWriteDeadline(t)
}

func (c *framedPacketConn) Read(p []byte) (n int, err error) {
	n, _, err = c.ReadFrom(p)
	return
}

func (c *framedPacketConn) Write(p []byte) (n int, err error) {
	return c.WriteTo(p, nil)
}

func (c *framedPacketConn) RemoteAddr() net.Addr {
	return nil
}

func createUDPLoopbackPair() (cronetFD int, proxyConn net.PacketConn, err error) {
	proxyAddress, err := net.ResolveUDPAddr("udp", "127.0.0.1:0")
	if err != nil {
		return -1, nil, err
	}
	proxyUDPConn, err := net.ListenUDP("udp", proxyAddress)
	if err != nil {
		return -1, nil, err
	}
	proxyLocalAddress := proxyUDPConn.LocalAddr().(*net.UDPAddr)

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

	proxyRawConn, err := proxyUDPConn.SyscallConn()
	if err != nil {
		cronetConn.Close()
		proxyUDPConn.Close()
		return -1, nil, err
	}

	var connectError error
	err = proxyRawConn.Control(func(fd uintptr) {
		sockaddr := &windows.SockaddrInet4{Port: cronetLocalAddress.Port}
		copy(sockaddr.Addr[:], cronetLocalAddress.IP.To4())
		connectError = windows.Connect(windows.Handle(fd), sockaddr)
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

	cronetRawConn, err := cronetConn.SyscallConn()
	if err != nil {
		cronetConn.Close()
		proxyUDPConn.Close()
		return -1, nil, err
	}

	var cronetHandle windows.Handle
	var duplicateError error
	err = cronetRawConn.Control(func(fd uintptr) {
		currentProcess, processError := windows.GetCurrentProcess()
		if processError != nil {
			duplicateError = processError
			return
		}
		duplicateError = windows.DuplicateHandle(
			currentProcess,
			windows.Handle(fd),
			currentProcess,
			&cronetHandle,
			0,
			false,
			windows.DUPLICATE_SAME_ACCESS,
		)
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

	return int(cronetHandle), proxyUDPConn, nil
}
