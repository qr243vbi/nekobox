package cache

// Modified by https://github.com/die-net/lrucache

import (
	"sync"
	"time"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/x/list"
)

type Option[K comparable, V any] func(*LruCache[K, V])

type EvictCallback[K comparable, V any] func(key K, value V)

func WithEvict[K comparable, V any](cb EvictCallback[K, V]) Option[K, V] {
	return func(l *LruCache[K, V]) {
		l.onEvict = cb
	}
}

func WithUpdateAgeOnGet[K comparable, V any]() Option[K, V] {
	return func(l *LruCache[K, V]) {
		l.updateAgeOnGet = true
	}
}

func WithAge[K comparable, V any](maxAge int64) Option[K, V] {
	return func(l *LruCache[K, V]) {
		l.maxAge = maxAge
	}
}

func WithSize[K comparable, V any](maxSize int) Option[K, V] {
	return func(l *LruCache[K, V]) {
		l.maxSize = maxSize
	}
}

func WithStale[K comparable, V any](stale bool) Option[K, V] {
	return func(l *LruCache[K, V]) {
		l.staleReturn = stale
	}
}

type LruCache[K comparable, V any] struct {
	maxAge         int64
	maxSize        int
	mu             sync.Mutex
	cache          map[K]*list.Element[*entry[K, V]]
	lru            list.List[*entry[K, V]] // Front is least-recent
	updateAgeOnGet bool
	staleReturn    bool
	onEvict        EvictCallback[K, V]
}

func New[K comparable, V any](options ...Option[K, V]) *LruCache[K, V] {
	lc := &LruCache[K, V]{
		cache: make(map[K]*list.Element[*entry[K, V]]),
	}

	for _, option := range options {
		option(lc)
	}

	return lc
}

func (c *LruCache[K, V]) Load(key K) (V, bool) {
	entry := c.get(key)
	if entry == nil {
		return common.DefaultValue[V](), false
	}
	value := entry.value

	return value, true
}

func (c *LruCache[K, V]) LoadOrStore(key K, constructor func() V) (V, bool) {
	c.mu.Lock()
	defer c.mu.Unlock()

	le, ok := c.cache[key]
	if ok {
		if c.maxAge > 0 && le.Value.expires <= time.Now().Unix() {
			c.deleteElement(le)
			goto create
		}

		c.lru.MoveToBack(le)
		entry := le.Value
		if c.maxAge > 0 && c.updateAgeOnGet {
			entry.expires = time.Now().Unix() + c.maxAge
		}
		return entry.value, true
	}

create:
	value := constructor()
	if le, ok := c.cache[key]; ok {
		c.lru.MoveToBack(le)
		e := le.Value
		e.value = value
		e.expires = time.Now().Unix() + c.maxAge
	} else {
		e := &entry[K, V]{key: key, value: value, expires: time.Now().Unix() + c.maxAge}
		c.cache[key] = c.lru.PushBack(e)
	}

	c.maybeDeleteOldest()
	return value, false
}

func (c *LruCache[K, V]) LoadOrStoreWithAge(key K, maxAge int64, constructor func() V) (V, bool) {
	c.mu.Lock()
	defer c.mu.Unlock()

	if maxAge == 0 {
		maxAge = c.maxAge
	}

	le, ok := c.cache[key]
	if ok {
		if c.maxAge > 0 && le.Value.expires <= time.Now().Unix() {
			c.deleteElement(le)
			goto create
		}

		c.lru.MoveToBack(le)
		entry := le.Value
		if c.maxAge > 0 && c.updateAgeOnGet {
			entry.expires = time.Now().Unix() + maxAge
		}
		return entry.value, true
	}

create:
	value := constructor()
	if le, ok := c.cache[key]; ok {
		c.lru.MoveToBack(le)
		e := le.Value
		e.value = value
		e.expires = time.Now().Unix() + maxAge
	} else {
		e := &entry[K, V]{key: key, value: value, expires: time.Now().Unix() + c.maxAge}
		c.cache[key] = c.lru.PushBack(e)
	}

	c.maybeDeleteOldest()
	return value, false
}

func (c *LruCache[K, V]) LoadWithExpire(key K) (V, time.Time, bool) {
	entry := c.get(key)
	if entry == nil {
		return common.DefaultValue[V](), time.Time{}, false
	}

	return entry.value, time.Unix(entry.expires, 0), true
}

func (c *LruCache[K, V]) Exist(key K) bool {
	c.mu.Lock()
	defer c.mu.Unlock()

	_, ok := c.cache[key]
	return ok
}

func (c *LruCache[K, V]) Store(key K, value V) {
	expires := int64(0)
	if c.maxAge > 0 {
		expires = time.Now().Unix() + c.maxAge
	}
	c.StoreWithExpire(key, value, time.Unix(expires, 0))
}

func (c *LruCache[K, V]) StoreWithExpire(key K, value V, expires time.Time) {
	c.mu.Lock()
	defer c.mu.Unlock()

	if le, ok := c.cache[key]; ok {
		c.lru.MoveToBack(le)
		e := le.Value
		e.value = value
		e.expires = expires.Unix()
	} else {
		e := &entry[K, V]{key: key, value: value, expires: expires.Unix()}
		c.cache[key] = c.lru.PushBack(e)
	}

	c.maybeDeleteOldest()
}

func (c *LruCache[K, V]) CloneTo(n *LruCache[K, V]) {
	c.mu.Lock()
	defer c.mu.Unlock()

	n.mu.Lock()
	defer n.mu.Unlock()

	n.lru = list.List[*entry[K, V]]{}
	n.cache = make(map[K]*list.Element[*entry[K, V]])

	for e := c.lru.Front(); e != nil; e = e.Next() {
		elm := e.Value
		n.cache[elm.key] = n.lru.PushBack(elm)
	}
}

func (c *LruCache[K, V]) Range(block func(key K, value V)) {
	c.mu.Lock()
	defer c.mu.Unlock()
	for le := c.lru.Front(); le != nil; le = le.Next() {
		block(le.Value.key, le.Value.value)
	}
}

func (c *LruCache[K, V]) get(key K) *entry[K, V] {
	c.mu.Lock()
	defer c.mu.Unlock()

	le, ok := c.cache[key]
	if !ok {
		return nil
	}

	if !c.staleReturn && c.maxAge > 0 && le.Value.expires <= time.Now().Unix() {
		c.deleteElement(le)
		c.maybeDeleteOldest()

		return nil
	}

	c.lru.MoveToBack(le)
	entry := le.Value
	if c.maxAge > 0 && c.updateAgeOnGet {
		entry.expires = time.Now().Unix() + c.maxAge
	}
	return entry
}

// Delete removes the value associated with a key.
func (c *LruCache[K, V]) Delete(key K) {
	c.mu.Lock()

	if le, ok := c.cache[key]; ok {
		c.deleteElement(le)
	}

	c.mu.Unlock()
}

func (c *LruCache[K, V]) Clear() {
	c.mu.Lock()
	defer c.mu.Unlock()
	for element := c.lru.Front(); element != nil; element = element.Next() {
		c.deleteElement(element)
	}
}

func (c *LruCache[K, V]) maybeDeleteOldest() {
	if !c.staleReturn && c.maxAge > 0 {
		now := time.Now().Unix()
		for le := c.lru.Front(); le != nil && le.Value.expires <= now; le = c.lru.Front() {
			c.deleteElement(le)
		}
	}

	if c.maxSize > 0 {
		if n := c.lru.Len(); n > c.maxSize {
			c.deleteElement(c.lru.Front())
		}
	}
}

func (c *LruCache[K, V]) deleteElement(le *list.Element[*entry[K, V]]) {
	c.lru.Remove(le)
	e := le.Value
	delete(c.cache, e.key)
	if c.onEvict != nil {
		c.onEvict(e.key, e.value)
	}
}

type entry[K comparable, V any] struct {
	key     K
	value   V
	expires int64
}
