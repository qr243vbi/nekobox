//go:build !linux

package control

import (
	"time"
)

func SetKeepAlivePeriod(idle time.Duration, interval time.Duration) Func {
	return nil
}
