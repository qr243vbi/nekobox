//go:build with_purego && windows && (amd64 || arm64)

package cronet

// registerFloatFuncs registers functions that use float types.
// Only supported on 64-bit platforms due to purego limitations.
func registerFloatFuncs() error {
	if err := registerFunc(&cronetEngineParamsNetworkThreadPrioritySet, "Cronet_EngineParams_network_thread_priority_set"); err != nil {
		return err
	}
	if err := registerFunc(&cronetEngineParamsNetworkThreadPriorityGet, "Cronet_EngineParams_network_thread_priority_get"); err != nil {
		return err
	}
	return nil
}
