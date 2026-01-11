package ntp

//go:generate go run golang.org/x/sys/windows/mkwinsyscall -output zsyscall_windows.go syscall_windows.go

// https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-setsystemtime
//sys setSystemTime(lpSystemTime *windows.Systemtime) (err error) = kernel32.SetSystemTime
