//go:build with_purego && windows

package cronet

import "runtime"

// EngineStartWithParams starts the engine on Windows.
// On Windows, we lock the OS thread to ensure stable thread-local storage
// state during Chromium's initialization, which creates threads and message loops.
func EngineStartWithParams(engine, params uintptr) int32 {
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()
	return cronetEngineStartWithParams(engine, params)
}
