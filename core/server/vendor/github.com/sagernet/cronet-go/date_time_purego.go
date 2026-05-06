//go:build with_purego

package cronet

import (
	"time"

	"github.com/sagernet/cronet-go/internal/cronet"
)

func NewDateTime() DateTime {
	return DateTime{cronet.DateTimeCreate()}
}

func (d DateTime) Destroy() {
	cronet.DateTimeDestroy(d.ptr)
}

// SetValue sets the number of milliseconds since the UNIX epoch.
func (d DateTime) SetValue(value time.Time) {
	cronet.DateTimeValueSet(d.ptr, value.UnixMilli())
}

func (d DateTime) Value() time.Time {
	return time.UnixMilli(cronet.DateTimeValueGet(d.ptr))
}
