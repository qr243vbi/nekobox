package pause

import "github.com/sagernet/sing/common/x/list"

type Manager interface {
	DevicePause()
	DeviceWake()
	NetworkPause()
	NetworkWake()
	IsDevicePaused() bool
	IsNetworkPaused() bool
	IsPaused() bool
	WaitActive()
	RegisterCallback(callback Callback) *list.Element[Callback]
	UnregisterCallback(element *list.Element[Callback])
}

const (
	EventDevicePaused int = iota
	EventDeviceWake
	EventNetworkPause
	EventNetworkWake
)

type Callback = func(event int)
