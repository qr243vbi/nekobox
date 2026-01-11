package memory

import "runtime"

func Total() uint64 {
	if nativeAvailable {
		return usageNative()
	}
	return Inuse()
}

func Inuse() uint64 {
	var memStats runtime.MemStats
	runtime.ReadMemStats(&memStats)
	return memStats.StackInuse + memStats.HeapInuse + memStats.HeapIdle - memStats.HeapReleased
}
