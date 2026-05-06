package collections

type Map[K comparable, V any] interface {
	Size() int
	IsEmpty() bool
	ContainsKey(key K) bool
	Get(key K) (V, bool)
	Put(key K, value V) V
	Remove(key K) bool
	PutAll(other Map[K, V])
	Clear()
	Keys() []K
	Values() []V
	Entries() []MapEntry[K, V]
}

type MapEntry[K comparable, V any] struct {
	Key   K
	Value V
}
