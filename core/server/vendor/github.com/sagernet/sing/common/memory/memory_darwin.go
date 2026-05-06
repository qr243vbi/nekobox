package memory

// #include <mach/mach.h>
import "C"
import "unsafe"

const nativeAvailable = true

func usageNative() uint64 {
	var memoryUsageInByte uint64
	var vmInfo C.task_vm_info_data_t
	var count C.mach_msg_type_number_t = C.TASK_VM_INFO_COUNT
	var kernelReturn C.kern_return_t = C.task_info(C.vm_map_t(C.mach_task_self_), C.TASK_VM_INFO, (*C.integer_t)(unsafe.Pointer(&vmInfo)), &count)
	if kernelReturn == C.KERN_SUCCESS {
		memoryUsageInByte = uint64(vmInfo.phys_footprint)
	}
	return memoryUsageInByte
}
