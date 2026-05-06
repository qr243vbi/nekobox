//go:build go1.21

package common

import (
	"cmp"
)

func Min[T cmp.Ordered](x, y T) T {
	return min(x, y)
}

func Max[T cmp.Ordered](x, y T) T {
	return max(x, y)
}
