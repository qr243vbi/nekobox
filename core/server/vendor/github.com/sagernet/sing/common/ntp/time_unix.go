//go:build linux || darwin

package ntp

import (
	"time"

	"golang.org/x/sys/unix"
)

func SetSystemTime(nowTime time.Time) error {
	timeVal := unix.NsecToTimeval(nowTime.UnixNano())
	return unix.Settimeofday(&timeVal)
}
