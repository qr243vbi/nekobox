//go:build with_purego && (386 || arm || mipsle)

package cronet

// EngineParamsNetworkThreadPrioritySet is not supported on 32-bit platforms.
// purego does not support float parameters on 32-bit platforms.
func EngineParamsNetworkThreadPrioritySet(params uintptr, priority float64) {
	panic("cronet: NetworkThreadPriority not supported on 32-bit platforms")
}

// EngineParamsNetworkThreadPriorityGet is not supported on 32-bit platforms.
// purego does not support float parameters on 32-bit platforms.
func EngineParamsNetworkThreadPriorityGet(params uintptr) float64 {
	panic("cronet: NetworkThreadPriority not supported on 32-bit platforms")
}
