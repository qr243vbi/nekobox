//go:build with_purego && (amd64 || arm64 || loong64 || mips64le || riscv64)

package cronet

func EngineParamsNetworkThreadPrioritySet(params uintptr, priority float64) {
	ensureLoaded()
	cronetEngineParamsNetworkThreadPrioritySet(params, priority)
}

func EngineParamsNetworkThreadPriorityGet(params uintptr) float64 {
	ensureLoaded()
	return cronetEngineParamsNetworkThreadPriorityGet(params)
}
