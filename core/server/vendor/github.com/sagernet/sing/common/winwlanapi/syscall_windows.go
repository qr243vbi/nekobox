//go:build windows

package winwlanapi

//go:generate go run golang.org/x/sys/windows/mkwinsyscall -output zsyscall_windows.go syscall_windows.go

// https://learn.microsoft.com/en-us/windows/win32/api/wlanapi/nf-wlanapi-wlanopenhandle
//sys wlanOpenHandle(clientVersion uint32, reserved uintptr, negotiatedVersion *uint32, clientHandle *windows.Handle) (ret uint32) = wlanapi.WlanOpenHandle

// https://learn.microsoft.com/en-us/windows/win32/api/wlanapi/nf-wlanapi-wlanclosehandle
//sys wlanCloseHandle(clientHandle windows.Handle, reserved uintptr) (ret uint32) = wlanapi.WlanCloseHandle

// https://learn.microsoft.com/en-us/windows/win32/api/wlanapi/nf-wlanapi-wlanenuminterfaces
//sys wlanEnumInterfaces(clientHandle windows.Handle, reserved uintptr, interfaceList **InterfaceInfoList) (ret uint32) = wlanapi.WlanEnumInterfaces

// https://learn.microsoft.com/en-us/windows/win32/api/wlanapi/nf-wlanapi-wlanqueryinterface
//sys wlanQueryInterface(clientHandle windows.Handle, interfaceGuid *windows.GUID, opCode uint32, reserved uintptr, dataSize *uint32, data *uintptr, opcodeValueType *uint32) (ret uint32) = wlanapi.WlanQueryInterface

// https://learn.microsoft.com/en-us/windows/win32/api/wlanapi/nf-wlanapi-wlanfreememory
//sys wlanFreeMemory(memory uintptr) = wlanapi.WlanFreeMemory

// https://learn.microsoft.com/en-us/windows/win32/api/wlanapi/nf-wlanapi-wlanregisternotification
//sys wlanRegisterNotification(clientHandle windows.Handle, notificationSource uint32, ignoreDuplicate bool, callback uintptr, callbackContext uintptr, reserved uintptr, prevNotificationSource *uint32) (ret uint32) = wlanapi.WlanRegisterNotification
