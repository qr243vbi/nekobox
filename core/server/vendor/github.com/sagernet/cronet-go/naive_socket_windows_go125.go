//go:build windows && go1.25

package cronet

import (
	"crypto/rand"
	"encoding/hex"
	"net"
	"os"
	"path/filepath"
	"strconv"

	"golang.org/x/sys/windows"
)

var (
	winsockSystemLibrary = windows.NewLazySystemDLL("ws2_32.dll")
	winsockProcAccept    = winsockSystemLibrary.NewProc("accept")
)

func createSocketPair() (cronetFD int, proxyConn net.Conn, err error) {
	cronetFD, proxyConn, err = createUnixSocketPair()
	if err == nil {
		return
	}
	return createTCPLoopbackSocketPair()
}

func createUnixSocketPair() (cronetFD int, proxyConn net.Conn, err error) {
	socketSuffix, err := randomHexString(8)
	if err != nil {
		return -1, nil, err
	}
	socketBaseName := "cronet-go-" + strconv.Itoa(os.Getpid()) + "-" + socketSuffix + ".sock"

	candidates := []string{
		"@" + socketBaseName,
	}

	temporaryPathCandidate := filepath.Join(os.TempDir(), socketBaseName)
	if len(temporaryPathCandidate) < windows.UNIX_PATH_MAX {
		candidates = append(candidates, temporaryPathCandidate)
	}

	var lastError error
	for _, name := range candidates {
		cronetFD, proxyConn, lastError = createUnixSocketPairWithName(name)
		if lastError == nil {
			return cronetFD, proxyConn, nil
		}
	}
	return -1, nil, lastError
}

func createUnixSocketPairWithName(name string) (cronetFD int, proxyConn net.Conn, err error) {
	if name != "" && name[0] != '@' {
		_ = os.Remove(name)
	}

	listenerSocket, err := windows.Socket(windows.AF_UNIX, windows.SOCK_STREAM, 0)
	if err != nil {
		return -1, nil, err
	}
	listenerClosed := false
	closeListenerSocket := func() {
		if listenerClosed {
			return
		}
		listenerClosed = true
		_ = windows.Closesocket(listenerSocket)
	}
	defer closeListenerSocket()

	listenerAddress := &windows.SockaddrUnix{Name: name}
	err = windows.Bind(listenerSocket, listenerAddress)
	if err != nil {
		return -1, nil, err
	}
	err = windows.Listen(listenerSocket, 1)
	if err != nil {
		return -1, nil, err
	}

	clientSocket, err := windows.Socket(windows.AF_UNIX, windows.SOCK_STREAM, 0)
	if err != nil {
		return -1, nil, err
	}
	// setUnixSocketBufferSize(clientSocket) // ineffective on Windows AF_UNIX
	closeClientSocket := func() {
		_ = windows.Closesocket(clientSocket)
	}

	acceptedDone := make(chan struct{})
	var acceptedSocket windows.Handle
	var acceptError error
	go func() {
		defer close(acceptedDone)
		r1, _, callError := winsockProcAccept.Call(uintptr(listenerSocket), 0, 0)
		if uintptr(r1) == uintptr(^uintptr(0)) {
			acceptError = callError
			return
		}
		acceptedSocket = windows.Handle(r1)
	}()

	connectError := windows.Connect(clientSocket, listenerAddress)
	if connectError != nil {
		closeListenerSocket()
		closeClientSocket()
		<-acceptedDone
		if acceptedSocket != 0 {
			_ = windows.Closesocket(acceptedSocket)
		}
		return -1, nil, connectError
	}

	<-acceptedDone
	if acceptError != nil {
		closeClientSocket()
		return -1, nil, acceptError
	}

	// setUnixSocketBufferSize(acceptedSocket) // ineffective on Windows AF_UNIX
	closeListenerSocket()
	if name != "" && name[0] != '@' {
		_ = os.Remove(name)
	}

	file := os.NewFile(uintptr(acceptedSocket), "unix")
	proxyConn, err = net.FileConn(file)
	file.Close()
	if err != nil {
		closeClientSocket()
		return -1, nil, err
	}

	return int(clientSocket), proxyConn, nil
}

// setUnixSocketBufferSize is ineffective on Windows AF_UNIX sockets.
// SO_SNDBUF/SO_RCVBUF are silently ignored.
//
// func setUnixSocketBufferSize(socket windows.Handle) {
// 	const bufferSize = 2 * 1024 * 1024
// 	_ = windows.SetsockoptInt(socket, windows.SOL_SOCKET, windows.SO_SNDBUF, bufferSize)
// 	_ = windows.SetsockoptInt(socket, windows.SOL_SOCKET, windows.SO_RCVBUF, bufferSize)
// }

func randomHexString(byteCount int) (string, error) {
	randomBytes := make([]byte, byteCount)
	_, err := rand.Read(randomBytes)
	if err != nil {
		return "", err
	}
	return hex.EncodeToString(randomBytes), nil
}
