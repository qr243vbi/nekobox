//go:build (darwin && !cgo) || !darwin

package memory

const nativeAvailable = false

func usageNative() uint64 {
	return 0
}
