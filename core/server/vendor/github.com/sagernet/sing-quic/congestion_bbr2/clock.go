// BBRv2 clock interface
// src from: https://github.com/cloudflare/quiche

package congestion_bbr2

import (
	"time"

	"github.com/sagernet/quic-go/monotime"
)

type Clock interface {
	Now() monotime.Time
}

type DefaultClock struct {
	TimeFunc func() time.Time
}

func (c DefaultClock) Now() monotime.Time {
	if c.TimeFunc != nil {
		return monotime.Time(c.TimeFunc().UnixNano())
	}
	return monotime.Now()
}
