package pause

import (
	"time"

	"github.com/sagernet/sing/common/x/list"
)

func RegisterTicker(manager Manager, ticker *time.Ticker, duration time.Duration, resume func()) *list.Element[Callback] {
	if manager.IsPaused() {
		ticker.Stop()
	}
	return manager.RegisterCallback(func(event int) {
		switch event {
		case EventDevicePaused, EventNetworkPause:
			ticker.Stop()
		case EventDeviceWake, EventNetworkWake:
			if resume != nil {
				resume()
			}
			ticker.Reset(duration)
		}
	})
}
