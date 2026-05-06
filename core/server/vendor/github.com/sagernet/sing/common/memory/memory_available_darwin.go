package memory

// #include <stddef.h>
// #include <dlfcn.h>
//
// static size_t get_available_memory(int *supported) {
//     typedef size_t (*proc_available_memory_func)(void);
//     static int resolved = 0;
//     static proc_available_memory_func fn = NULL;
//     if (!resolved) {
//         fn = (proc_available_memory_func)dlsym(RTLD_DEFAULT, "os_proc_available_memory");
//         resolved = 1;
//     }
//     if (fn) {
//         *supported = 1;
//         return fn();
//     }
//     *supported = 0;
//     return 0;
// }
import "C"

func availableNative() uint64 {
	var supported C.int
	result := C.get_available_memory(&supported)
	if supported == 0 {
		return 0
	}
	return uint64(result)
}

func availableNativeSupported() bool {
	var supported C.int
	C.get_available_memory(&supported)
	return supported != 0
}
