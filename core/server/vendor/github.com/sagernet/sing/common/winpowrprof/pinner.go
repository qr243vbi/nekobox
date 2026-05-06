//go:build go1.21

package winpowrprof

import "runtime"

type myPinner = runtime.Pinner
