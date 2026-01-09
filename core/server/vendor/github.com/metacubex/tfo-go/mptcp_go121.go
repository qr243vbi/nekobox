//go:build go1.21

package tfo

import "net"

func multipathTCP(d net.Dialer) bool {
	return d.MultipathTCP()
}

func setMultipathTCP(d net.Dialer, use bool) {
	d.SetMultipathTCP(use)
}
