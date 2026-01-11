package tfo

// SetTFOListener enables TCP Fast Open on the listener.
// On platforms where a backlog argument is required, Go std's listen(2) backlog is used.
// To specify a custom backlog, use [SetTFOListenerWithBacklog].
func SetTFOListener(fd uintptr) error {
	return setTFOListener(fd) // sockopt_linux.go, sockopt_listen_generic.go, sockopt_stub.go
}

// SetTFOListenerWithBacklog enables TCP Fast Open on the listener with the given backlog.
// If the backlog is 0, Go std's listen(2) backlog is used.
// If the platform does not support custom backlog values, the given backlog is ignored.
func SetTFOListenerWithBacklog(fd uintptr, backlog int) error {
	return setTFOListenerWithBacklog(fd, backlog) // sockopt_linux.go, sockopt_listen_generic.go, sockopt_stub.go
}

// SetTFODialer enables TCP Fast Open on the dialer.
func SetTFODialer(fd uintptr) error {
	return setTFODialer(fd) // sockopt_darwin.go, sockopt_linux.go, sockopt_connect_generic.go, sockopt_stub.go
}
