package netx

import (
	"net"
	"strconv"
	"sync"
	"time"
)

// Copied and modified from src/net/interface.go

// IPv6ZoneCache represents a cache holding partial network
// interface information. It is used for reducing the cost of IPv6
// addressing scope zone resolution.
//
// Multiple names sharing the index are managed by first-come
// first-served basis for consistency.
type IPv6ZoneCache struct {
	sync.RWMutex                // guard the following
	lastFetched  time.Time      // last time routing information was fetched
	toIndex      map[string]int // interface name to its index
	toName       map[int]string // interface index to its name
}

// ZoneCache is the global shared cache for IPv6 addressing scope zone resolution.
var ZoneCache = IPv6ZoneCache{
	toIndex: make(map[string]int),
	toName:  make(map[int]string),
}

// Update refreshes the network interface information if the cache was last
// updated more than 1 minute ago, or if force is set. It reports whether the
// cache was updated.
func (zc *IPv6ZoneCache) Update(ift []net.Interface, force bool) (updated bool) {
	zc.Lock()
	defer zc.Unlock()
	now := time.Now()
	if !force && zc.lastFetched.After(now.Add(-60*time.Second)) {
		return false
	}
	zc.lastFetched = now
	if len(ift) == 0 {
		var err error
		if ift, err = net.Interfaces(); err != nil {
			return false
		}
	}
	zc.toIndex = make(map[string]int, len(ift))
	zc.toName = make(map[int]string, len(ift))
	for _, ifi := range ift {
		if ifi.Name != "" {
			zc.toIndex[ifi.Name] = ifi.Index
			if _, ok := zc.toName[ifi.Index]; !ok {
				zc.toName[ifi.Index] = ifi.Name
			}
		}
	}
	return true
}

// Name returns the name of the network interface with the given index.
func (zc *IPv6ZoneCache) Name(index int) string {
	if index == 0 {
		return ""
	}
	return zc.name(index)
}

func (zc *IPv6ZoneCache) name(index int) string {
	updated := ZoneCache.Update(nil, false)
	ZoneCache.RLock()
	name, ok := ZoneCache.toName[index]
	ZoneCache.RUnlock()
	if !ok && !updated {
		ZoneCache.Update(nil, true)
		ZoneCache.RLock()
		name, ok = ZoneCache.toName[index]
		ZoneCache.RUnlock()
	}
	if !ok { // last resort
		name = strconv.Itoa(index)
	}
	return name
}

// Index returns the Index of the network interface with the given name.
func (zc *IPv6ZoneCache) Index(name string) int {
	if name == "" {
		return 0
	}
	return zc.index(name)
}

func (zc *IPv6ZoneCache) index(name string) int {
	updated := ZoneCache.Update(nil, false)
	ZoneCache.RLock()
	index, ok := ZoneCache.toIndex[name]
	ZoneCache.RUnlock()
	if !ok && !updated {
		ZoneCache.Update(nil, true)
		ZoneCache.RLock()
		index, ok = ZoneCache.toIndex[name]
		ZoneCache.RUnlock()
	}
	if !ok { // last resort
		index, _ = strconv.Atoi(name)
	}
	return index
}
