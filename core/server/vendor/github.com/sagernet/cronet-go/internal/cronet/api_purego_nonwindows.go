//go:build with_purego && !windows

package cronet

// EngineStartWithParams starts the engine on non-Windows platforms.
func EngineStartWithParams(engine, params uintptr) int32 {
	return cronetEngineStartWithParams(engine, params)
}
