//go:build darwin || freebsd

package tfo

func setTFODialerFromSocket(fd uintptr) error {
	return setTFODialer(fd)
}

// doConnectCanFallback returns whether err from [doConnect] indicates lack of TFO support.
func doConnectCanFallback(_ error) bool {
	return false
}
