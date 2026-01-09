package winpowrprof

import "unsafe"

//go:generate go run golang.org/x/sys/windows/mkwinsyscall -output zsyscall_windows.go syscall_windows.go

const DEVICE_NOTIFY_CALLBACK = 2

type DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS struct {
	callback uintptr
	context  unsafe.Pointer
}

// https://learn.microsoft.com/en-us/windows/win32/api/powerbase/nf-powerbase-powerregistersuspendresumenotification
//sys PowerRegisterSuspendResumeNotification(flags uint32, recipient *DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS, registrationHandle *uintptr) (ret error) = powrprof.PowerRegisterSuspendResumeNotification

// https://learn.microsoft.com/en-us/windows/win32/api/powerbase/nf-powerbase-powerunregistersuspendresumenotification
//sys PowerUnregisterSuspendResumeNotification(handle uintptr) (ret error) = powrprof.PowerUnregisterSuspendResumeNotification

const (
	PBT_APMSUSPEND         uint32 = 4
	PBT_APMRESUMESUSPEND   uint32 = 7
	PBT_APMRESUMEAUTOMATIC uint32 = 18
)
