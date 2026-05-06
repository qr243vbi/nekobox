//go:build !go1.21

package common

func ClearArray[T ~[]E, E any](t T) {
	var defaultValue E
	for i := range t {
		t[i] = defaultValue
	}
}

func ClearMap[T ~map[K]V, K comparable, V any](t T) {
	for k := range t {
		delete(t, k)
	}
}
