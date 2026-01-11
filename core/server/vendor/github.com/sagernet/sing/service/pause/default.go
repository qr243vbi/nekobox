package pause

import (
	"context"
	"sync"
	"sync/atomic"

	"github.com/sagernet/sing/common/x/list"
	"github.com/sagernet/sing/service"
)

type defaultManager struct {
	ctx           context.Context
	access        sync.Mutex
	devicePause   chan struct{}
	devicePaused  atomic.Bool
	networkPause  chan struct{}
	networkPaused atomic.Bool
	callbacks     list.List[Callback]
}

func WithDefaultManager(ctx context.Context) context.Context {
	if service.FromContext[Manager](ctx) != nil {
		return ctx
	}
	devicePauseChan := make(chan struct{})
	networkPauseChan := make(chan struct{})
	close(devicePauseChan)
	close(networkPauseChan)
	manager := &defaultManager{
		ctx:          ctx,
		devicePause:  devicePauseChan,
		networkPause: networkPauseChan,
	}
	return service.ContextWith[Manager](ctx, manager)
}

func (d *defaultManager) DevicePause() {
	d.access.Lock()
	defer d.access.Unlock()
	select {
	case <-d.devicePause:
		d.devicePaused.Store(true)
		d.devicePause = make(chan struct{})
		d.emit(EventDevicePaused)
	default:
	}
}

func (d *defaultManager) DeviceWake() {
	d.access.Lock()
	defer d.access.Unlock()
	select {
	case <-d.devicePause:
	default:
		d.devicePaused.Store(false)
		close(d.devicePause)
		d.emit(EventDeviceWake)
	}
}

func (d *defaultManager) NetworkPause() {
	d.access.Lock()
	defer d.access.Unlock()
	select {
	case <-d.networkPause:
		d.networkPaused.Store(true)
		d.networkPause = make(chan struct{})
		d.emit(EventNetworkPause)
	default:
	}
}

func (d *defaultManager) NetworkWake() {
	d.access.Lock()
	defer d.access.Unlock()
	select {
	case <-d.networkPause:
	default:
		d.networkPaused.Store(false)
		close(d.networkPause)
		d.emit(EventNetworkWake)
	}
}

func (d *defaultManager) RegisterCallback(callback Callback) *list.Element[Callback] {
	d.access.Lock()
	defer d.access.Unlock()
	return d.callbacks.PushBack(callback)
}

func (d *defaultManager) UnregisterCallback(element *list.Element[Callback]) {
	d.access.Lock()
	defer d.access.Unlock()
	d.callbacks.Remove(element)
}

func (d *defaultManager) IsDevicePaused() bool {
	return d.devicePaused.Load()
}

func (d *defaultManager) IsNetworkPaused() bool {
	return d.networkPaused.Load()
}

func (d *defaultManager) IsPaused() bool {
	select {
	case <-d.devicePause:
	default:
		return true
	}

	select {
	case <-d.networkPause:
	default:
		return true
	}

	return false
}

func (d *defaultManager) WaitActive() {
	select {
	case <-d.devicePause:
	case <-d.ctx.Done():
	}

	select {
	case <-d.networkPause:
	case <-d.ctx.Done():
	}
}

func (d *defaultManager) emit(event int) {
	for element := d.callbacks.Front(); element != nil; element = element.Next() {
		element.Value(event)
	}
}
