//go:build with_purego

package cronet

import (
	"sync"
	"sync/atomic"

	"github.com/sagernet/cronet-go/internal/cronet"

	"github.com/ebitengine/purego"
)

type uploadDataProviderEntry struct {
	handler   UploadDataProviderHandler
	destroyed atomic.Bool
}

var (
	uploadDataAccess      sync.RWMutex
	uploadDataProviderMap map[uintptr]*uploadDataProviderEntry

	uploadDataProviderGetLength uintptr
	uploadDataProviderRead      uintptr
	uploadDataProviderRewind    uintptr
	uploadDataProviderClose     uintptr
)

func init() {
	uploadDataProviderMap = make(map[uintptr]*uploadDataProviderEntry)

	uploadDataProviderGetLength = purego.NewCallback(onGetLengthCallback)
	uploadDataProviderRead = purego.NewCallback(onReadCallback)
	uploadDataProviderRewind = purego.NewCallback(onRewindCallback)
	uploadDataProviderClose = purego.NewCallback(onCloseCallback)
}

func instanceOfUploadDataProvider(self uintptr) UploadDataProviderHandler {
	uploadDataAccess.RLock()
	defer uploadDataAccess.RUnlock()
	entry := uploadDataProviderMap[self]
	if entry == nil || entry.destroyed.Load() {
		return nil
	}
	return entry.handler
}

func onGetLengthCallback(self uintptr) uintptr {
	handler := instanceOfUploadDataProvider(self)
	if handler == nil {
		return 0
	}
	return uintptr(handler.Length(UploadDataProvider{self}))
}

func onReadCallback(self, sink, buffer uintptr) uintptr {
	handler := instanceOfUploadDataProvider(self)
	if handler == nil {
		return 0
	}
	handler.Read(
		UploadDataProvider{self},
		UploadDataSink{sink},
		Buffer{buffer},
	)
	return 0
}

func onRewindCallback(self, sink uintptr) uintptr {
	handler := instanceOfUploadDataProvider(self)
	if handler == nil {
		return 0
	}
	handler.Rewind(
		UploadDataProvider{self},
		UploadDataSink{sink},
	)
	return 0
}

func onCloseCallback(self uintptr) uintptr {
	handler := instanceOfUploadDataProvider(self)
	if handler == nil {
		return 0
	}
	handler.Close(UploadDataProvider{self})
	uploadDataAccess.Lock()
	delete(uploadDataProviderMap, self)
	uploadDataAccess.Unlock()
	return 0
}

func NewUploadDataProvider(handler UploadDataProviderHandler) UploadDataProvider {
	if handler == nil {
		panic("nil upload data provider handler")
	}
	ptr := cronet.UploadDataProviderCreateWith(
		uploadDataProviderGetLength,
		uploadDataProviderRead,
		uploadDataProviderRewind,
		uploadDataProviderClose,
	)
	uploadDataAccess.Lock()
	uploadDataProviderMap[ptr] = &uploadDataProviderEntry{handler: handler}
	uploadDataAccess.Unlock()
	return UploadDataProvider{ptr}
}

func (p UploadDataProvider) Destroy() {
	uploadDataAccess.RLock()
	entry := uploadDataProviderMap[p.ptr]
	uploadDataAccess.RUnlock()
	if entry != nil {
		entry.destroyed.Store(true)
	}
	cronet.UploadDataProviderDestroy(p.ptr)
}
