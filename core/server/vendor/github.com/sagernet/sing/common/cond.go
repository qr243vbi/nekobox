package common

import (
	"context"
	"io"
	"runtime"
	"sort"
	"unsafe"

	"github.com/sagernet/sing/common/x/constraints"
)

func Any[T any](array []T, block func(it T) bool) bool {
	for _, it := range array {
		if block(it) {
			return true
		}
	}
	return false
}

func AnyIndexed[T any](array []T, block func(index int, it T) bool) bool {
	for index, it := range array {
		if block(index, it) {
			return true
		}
	}
	return false
}

func All[T any](array []T, block func(it T) bool) bool {
	for _, it := range array {
		if !block(it) {
			return false
		}
	}
	return true
}

func AllIndexed[T any](array []T, block func(index int, it T) bool) bool {
	for index, it := range array {
		if !block(index, it) {
			return false
		}
	}
	return true
}

func Contains[T comparable](arr []T, target T) bool {
	for index := range arr {
		if target == arr[index] {
			return true
		}
	}
	return false
}

func Map[T any, N any](arr []T, block func(it T) N) []N {
	retArr := make([]N, 0, len(arr))
	for index := range arr {
		retArr = append(retArr, block(arr[index]))
	}
	return retArr
}

func MapIndexed[T any, N any](arr []T, block func(index int, it T) N) []N {
	retArr := make([]N, 0, len(arr))
	for index := range arr {
		retArr = append(retArr, block(index, arr[index]))
	}
	return retArr
}

func FlatMap[T any, N any](arr []T, block func(it T) []N) []N {
	var retAddr []N
	for _, item := range arr {
		retAddr = append(retAddr, block(item)...)
	}
	return retAddr
}

func FlatMapIndexed[T any, N any](arr []T, block func(index int, it T) []N) []N {
	var retAddr []N
	for index, item := range arr {
		retAddr = append(retAddr, block(index, item)...)
	}
	return retAddr
}

func Filter[T any](arr []T, block func(it T) bool) []T {
	var retArr []T
	for _, it := range arr {
		if block(it) {
			retArr = append(retArr, it)
		}
	}
	return retArr
}

func FilterNotNil[T any](arr []T) []T {
	return Filter(arr, func(it T) bool {
		var anyIt any = it
		return anyIt != nil
	})
}

func FilterNotDefault[T comparable](arr []T) []T {
	var defaultValue T
	return Filter(arr, func(it T) bool {
		return it != defaultValue
	})
}

func FilterIndexed[T any](arr []T, block func(index int, it T) bool) []T {
	var retArr []T
	for index, it := range arr {
		if block(index, it) {
			retArr = append(retArr, it)
		}
	}
	return retArr
}

func Find[T any](arr []T, block func(it T) bool) T {
	for _, it := range arr {
		if block(it) {
			return it
		}
	}
	return DefaultValue[T]()
}

func FindIndexed[T any](arr []T, block func(index int, it T) bool) T {
	for index, it := range arr {
		if block(index, it) {
			return it
		}
	}
	return DefaultValue[T]()
}

func Index[T any](arr []T, block func(it T) bool) int {
	for index, it := range arr {
		if block(it) {
			return index
		}
	}
	return -1
}

func IndexIndexed[T any](arr []T, block func(index int, it T) bool) int {
	for index, it := range arr {
		if block(index, it) {
			return index
		}
	}
	return -1
}

func Equal[S ~[]E, E comparable](s1, s2 S) bool {
	if len(s1) != len(s2) {
		return false
	}
	for i := range s1 {
		if s1[i] != s2[i] {
			return false
		}
	}
	return true
}

//go:norace
func Dup[T any](obj T) T {
	pointer := uintptr(unsafe.Pointer(&obj))
	//nolint:staticcheck
	//goland:noinspection GoVetUnsafePointer
	return *(*T)(unsafe.Pointer(pointer))
}

func KeepAlive(obj any) {
	runtime.KeepAlive(obj)
}

func Uniq[T comparable](arr []T) []T {
	result := make([]T, 0, len(arr))
	seen := make(map[T]struct{}, len(arr))

	for _, item := range arr {
		if _, ok := seen[item]; ok {
			continue
		}

		seen[item] = struct{}{}
		result = append(result, item)
	}

	return result
}

func UniqBy[T any, C comparable](arr []T, block func(it T) C) []T {
	result := make([]T, 0, len(arr))
	seen := make(map[C]struct{}, len(arr))

	for _, item := range arr {
		c := block(item)
		if _, ok := seen[c]; ok {
			continue
		}

		seen[c] = struct{}{}
		result = append(result, item)
	}

	return result
}

func SortBy[T any, C constraints.Ordered](arr []T, block func(it T) C) {
	sort.Slice(arr, func(i, j int) bool {
		return block(arr[i]) < block(arr[j])
	})
}

func MinBy[T any, C constraints.Ordered](arr []T, block func(it T) C) T {
	var min T
	var minValue C
	if len(arr) == 0 {
		return min
	}
	min = arr[0]
	minValue = block(min)
	for i := 1; i < len(arr); i++ {
		item := arr[i]
		value := block(item)
		if value < minValue {
			min = item
			minValue = value
		}
	}
	return min
}

func MaxBy[T any, C constraints.Ordered](arr []T, block func(it T) C) T {
	var max T
	var maxValue C
	if len(arr) == 0 {
		return max
	}
	max = arr[0]
	maxValue = block(max)
	for i := 1; i < len(arr); i++ {
		item := arr[i]
		value := block(item)
		if value > maxValue {
			max = item
			maxValue = value
		}
	}
	return max
}

func FilterIsInstance[T any, N any](arr []T, block func(it T) (N, bool)) []N {
	var retArr []N
	for _, it := range arr {
		if n, isN := block(it); isN {
			retArr = append(retArr, n)
		}
	}
	return retArr
}

func Reverse[T any](arr []T) []T {
	length := len(arr)
	half := length / 2

	for i := 0; i < half; i = i + 1 {
		j := length - 1 - i
		arr[i], arr[j] = arr[j], arr[i]
	}

	return arr
}

func ReverseMap[K comparable, V comparable](m map[K]V) map[V]K {
	ret := make(map[V]K, len(m))
	for k, v := range m {
		ret[v] = k
	}
	return ret
}

func Done(ctx context.Context) bool {
	select {
	case <-ctx.Done():
		return true
	default:
		return false
	}
}

func Error(_ any, err error) error {
	return err
}

func Must(errs ...error) {
	for _, err := range errs {
		if err != nil {
			panic(err)
		}
	}
}

func Must1[T any](result T, err error) T {
	if err != nil {
		panic(err)
	}
	return result
}

func Must2[T any, T2 any](result T, result2 T2, err error) (T, T2) {
	if err != nil {
		panic(err)
	}
	return result, result2
}

// Deprecated: use E.Errors
func AnyError(errs ...error) error {
	for _, err := range errs {
		if err != nil {
			return err
		}
	}
	return nil
}

func PtrOrNil[T any](ptr *T) any {
	if ptr == nil {
		return nil
	}
	return ptr
}

func PtrValueOrDefault[T any](ptr *T) T {
	if ptr == nil {
		return DefaultValue[T]()
	}
	return *ptr
}

func IsEmpty[T comparable](obj T) bool {
	return obj == DefaultValue[T]()
}

func DefaultValue[T any]() T {
	var defaultValue T
	return defaultValue
}

func Ptr[T any](obj T) *T {
	return &obj
}

func Close(closers ...any) error {
	var retErr error
	for _, closer := range closers {
		if closer == nil {
			continue
		}
		switch c := closer.(type) {
		case io.Closer:
			err := c.Close()
			if err != nil {
				retErr = err
			}
			continue
		case WithUpstream:
			err := Close(c.Upstream())
			if err != nil {
				retErr = err
			}
		}
	}
	return retErr
}
