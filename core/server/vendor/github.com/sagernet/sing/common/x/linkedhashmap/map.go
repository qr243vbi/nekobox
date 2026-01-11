package linkedhashmap

import (
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/x/collections"
	"github.com/sagernet/sing/common/x/list"
)

var _ collections.Map[string, any] = (*Map[string, any])(nil)

type Map[K comparable, V any] struct {
	raw    list.List[collections.MapEntry[K, V]]
	rawMap map[K]*list.Element[collections.MapEntry[K, V]]
}

func (m *Map[K, V]) init() {
	if m.rawMap == nil {
		m.rawMap = make(map[K]*list.Element[collections.MapEntry[K, V]])
	}
}

func (m *Map[K, V]) Size() int {
	return m.raw.Size()
}

func (m *Map[K, V]) IsEmpty() bool {
	return m.raw.IsEmpty()
}

func (m *Map[K, V]) ContainsKey(key K) bool {
	m.init()
	_, loaded := m.rawMap[key]
	return loaded
}

func (m *Map[K, V]) Get(key K) (V, bool) {
	m.init()
	value, loaded := m.rawMap[key]
	if loaded {
		return value.Value.Value, true
	} else {
		return common.DefaultValue[V](), false
	}
}

func (m *Map[K, V]) Put(key K, value V) V {
	m.init()
	entry, loaded := m.rawMap[key]
	if loaded {
		oldValue := entry.Value.Value
		entry.Value.Value = value
		return oldValue
	}
	entry = m.raw.PushBack(collections.MapEntry[K, V]{Key: key, Value: value})
	m.rawMap[key] = entry
	return common.DefaultValue[V]()
}

func (m *Map[K, V]) Remove(key K) bool {
	m.init()
	entry, loaded := m.rawMap[key]
	if !loaded {
		return false
	}
	m.raw.Remove(entry)
	delete(m.rawMap, key)
	return true
}

func (m *Map[K, V]) PutAll(other collections.Map[K, V]) {
	m.init()
	for _, item := range other.Entries() {
		entry, loaded := m.rawMap[item.Key]
		if loaded {
			entry.Value.Value = item.Value
			continue
		}
		entry = m.raw.PushBack(item)
		m.rawMap[item.Key] = entry
	}
}

func (m *Map[K, V]) Clear() {
	*m = Map[K, V]{}
}

func (m *Map[K, V]) Keys() []K {
	result := make([]K, 0, m.raw.Len())
	for item := m.raw.Front(); item != nil; item = item.Next() {
		result = append(result, item.Value.Key)
	}
	return result
}

func (m *Map[K, V]) Values() []V {
	result := make([]V, 0, m.raw.Len())
	for item := m.raw.Front(); item != nil; item = item.Next() {
		result = append(result, item.Value.Value)
	}
	return result
}

func (m *Map[K, V]) Entries() []collections.MapEntry[K, V] {
	result := make([]collections.MapEntry[K, V], 0, m.raw.Len())
	for item := m.raw.Front(); item != nil; item = item.Next() {
		result = append(result, item.Value)
	}
	return result
}
