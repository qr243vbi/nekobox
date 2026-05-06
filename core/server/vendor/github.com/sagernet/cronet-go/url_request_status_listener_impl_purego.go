//go:build with_purego

package cronet

import (
	"sync"

	"github.com/sagernet/cronet-go/internal/cronet"

	"github.com/ebitengine/purego"
)

var (
	urlRequestStatusListenerAccess           sync.RWMutex
	urlRequestStatusListenerMap              map[uintptr]URLRequestStatusListenerOnStatusFunc
	urlRequestStatusListenerOnStatusCallback uintptr
)

func init() {
	urlRequestStatusListenerMap = make(map[uintptr]URLRequestStatusListenerOnStatusFunc)
	urlRequestStatusListenerOnStatusCallback = purego.NewCallback(cronetURLRequestStatusListenerOnStatus)
}

func cronetURLRequestStatusListenerOnStatus(self uintptr, status int32) uintptr {
	urlRequestStatusListenerAccess.Lock()
	listener := urlRequestStatusListenerMap[self]
	delete(urlRequestStatusListenerMap, self) // One-shot callback
	urlRequestStatusListenerAccess.Unlock()
	if listener == nil {
		return 0 // Race with Destroy() or already invoked - silently return
	}
	listener(URLRequestStatusListener{self}, URLRequestStatusListenerStatus(status))
	return 0
}

func NewURLRequestStatusListener(onStatusFunc URLRequestStatusListenerOnStatusFunc) URLRequestStatusListener {
	if onStatusFunc == nil {
		panic("nil url request status listener function")
	}
	ptr := cronet.UrlRequestStatusListenerCreateWith(urlRequestStatusListenerOnStatusCallback)
	urlRequestStatusListenerAccess.Lock()
	urlRequestStatusListenerMap[ptr] = onStatusFunc
	urlRequestStatusListenerAccess.Unlock()
	return URLRequestStatusListener{ptr}
}

func (l URLRequestStatusListener) destroy() {
	urlRequestStatusListenerAccess.Lock()
	delete(urlRequestStatusListenerMap, l.ptr)
	urlRequestStatusListenerAccess.Unlock()
}
