//go:build !(windows || linux || darwin)

package ntp

import (
	"os"
	"time"
)

func SetSystemTime(nowTime time.Time) error {
	return os.ErrInvalid
}
