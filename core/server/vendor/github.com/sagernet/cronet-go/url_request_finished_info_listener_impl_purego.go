//go:build with_purego

package cronet

import (
	"sync"
	"sync/atomic"

	"github.com/sagernet/cronet-go/internal/cronet"

	"github.com/ebitengine/purego"
)

type urlRequestFinishedInfoListenerEntry struct {
	handler   URLRequestFinishedInfoListenerOnRequestFinishedFunc
	destroyed atomic.Bool
}

var (
	urlRequestFinishedInfoListenerAccess                    sync.RWMutex
	urlRequestFinishedInfoListenerMap                       map[uintptr]*urlRequestFinishedInfoListenerEntry
	urlRequestFinishedInfoListenerOnRequestFinishedCallback uintptr
)

func init() {
	urlRequestFinishedInfoListenerMap = make(map[uintptr]*urlRequestFinishedInfoListenerEntry)
	urlRequestFinishedInfoListenerOnRequestFinishedCallback = purego.NewCallback(cronetURLRequestFinishedInfoListenerOnRequestFinished)
}

func cronetURLRequestFinishedInfoListenerOnRequestFinished(self, requestInfo, responseInfo, errorPtr uintptr) uintptr {
	urlRequestFinishedInfoListenerAccess.RLock()
	entry := urlRequestFinishedInfoListenerMap[self]
	urlRequestFinishedInfoListenerAccess.RUnlock()
	if entry == nil || entry.destroyed.Load() {
		return 0 // Post-destroy callback, silently ignore
	}
	entry.handler(
		URLRequestFinishedInfoListener{self},
		RequestFinishedInfo{requestInfo},
		URLResponseInfo{responseInfo},
		Error{errorPtr},
	)
	return 0
}

func NewURLRequestFinishedInfoListener(finishedFunc URLRequestFinishedInfoListenerOnRequestFinishedFunc) URLRequestFinishedInfoListener {
	if finishedFunc == nil {
		panic("nil url request finished info listener function")
	}
	ptr := cronet.RequestFinishedInfoListenerCreateWith(urlRequestFinishedInfoListenerOnRequestFinishedCallback)
	urlRequestFinishedInfoListenerAccess.Lock()
	urlRequestFinishedInfoListenerMap[ptr] = &urlRequestFinishedInfoListenerEntry{handler: finishedFunc}
	urlRequestFinishedInfoListenerAccess.Unlock()
	return URLRequestFinishedInfoListener{ptr}
}

func (l URLRequestFinishedInfoListener) destroy() {
	urlRequestFinishedInfoListenerAccess.Lock()
	entry := urlRequestFinishedInfoListenerMap[l.ptr]
	if entry != nil {
		entry.destroyed.Store(true)
	}
	urlRequestFinishedInfoListenerAccess.Unlock()
	cronet.RequestFinishedInfoListenerDestroy(l.ptr)
	// Cleanup after C destroy
	urlRequestFinishedInfoListenerAccess.Lock()
	delete(urlRequestFinishedInfoListenerMap, l.ptr)
	urlRequestFinishedInfoListenerAccess.Unlock()
}
