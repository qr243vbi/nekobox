package replay

import (
	"sync"
	"time"
)

type SimpleFilter struct {
	access    sync.Mutex
	lastClean time.Time
	timeout   time.Duration
	pool      map[string]time.Time
}

func NewSimple(timeout time.Duration) Filter {
	return &SimpleFilter{
		lastClean: time.Now(),
		pool:      make(map[string]time.Time),
		timeout:   timeout,
	}
}

func (f *SimpleFilter) Check(salt []byte) bool {
	now := time.Now()
	saltStr := string(salt)
	f.access.Lock()
	defer f.access.Unlock()

	var exists bool
	if now.Sub(f.lastClean) > f.timeout {
		for oldSum, added := range f.pool {
			if now.Sub(added) > f.timeout {
				delete(f.pool, oldSum)
			}
		}
		_, exists = f.pool[saltStr]
		f.lastClean = now
	} else {
		if added, loaded := f.pool[saltStr]; loaded && now.Sub(added) <= f.timeout {
			exists = true
		}
	}
	if !exists {
		f.pool[saltStr] = now
	}
	return !exists
}
