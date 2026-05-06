//go:build !(darwin && cgo) && !linux

package memory

func availableNativeSupported() bool {
	return false
}

func availableNative() uint64 {
	return 0
}
