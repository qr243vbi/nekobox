//go:build !go1.21

package tfo

import "net"

func multipathTCP(d net.Dialer) bool {
	return false
}

func setMultipathTCP(d net.Dialer, use bool) {
	return
}
