package tfo

import (
	_ "unsafe"

	"golang.org/x/sys/unix"
)

// TCPFastopenQueueLength is the maximum number of total pending TFO connection requests,
// see https://datatracker.ietf.org/doc/html/rfc7413#section-5.1 for why this limit exists.
// The current value is the default net.core.somaxconn on Linux.
//
// Deprecated: This constant is no longer used in this module and will be removed in v3.
const TCPFastopenQueueLength = 4096

func setTFOListener(fd uintptr) error {
	return setTFOListenerWithBacklog(fd, 0)
}

func setTFOListenerWithBacklog(fd uintptr, backlog int) error {
	if backlog == 0 {
		backlog = listenerBacklog()
	}
	return setTFO(int(fd), backlog)
}

// listenerBacklog is linked from src/net/net.go
//
//go:linkname listenerBacklog net.listenerBacklog
func listenerBacklog() int

func setTFODialer(fd uintptr) error {
	return unix.SetsockoptInt(int(fd), unix.IPPROTO_TCP, unix.TCP_FASTOPEN_CONNECT, 1)
}
