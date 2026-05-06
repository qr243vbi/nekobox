//go:build go1.20 && !go1.21

package common

import "github.com/sagernet/sing/common/x/constraints"

func Min[T constraints.Ordered](x, y T) T {
	if x < y {
		return x
	}
	return y
}

func Max[T constraints.Ordered](x, y T) T {
	if x < y {
		return y
	}
	return x
}
