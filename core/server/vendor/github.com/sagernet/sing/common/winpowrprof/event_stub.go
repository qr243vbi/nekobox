//go:build !windows

package winpowrprof

import (
	"os"
)

func NewEventListener(callback EventCallback) (EventListener, error) {
	return nil, os.ErrInvalid
}
