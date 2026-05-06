//go:build !(linux || windows || darwin || freebsd)

package tun

import (
	"os"
)

func New(config Options) (Tun, error) {
	return nil, os.ErrInvalid
}
