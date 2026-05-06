//go:build windows

package winwlanapi

import (
	"os"
	"unsafe"

	"golang.org/x/sys/windows"
)

const (
	ClientVersion2 = 2

	// InterfaceOpcode for WlanQueryInterface
	IntfOpcodeCurrentConnection = 7

	// NotificationSource for WlanRegisterNotification
	NotificationSourceNone = 0
	NotificationSourceACM  = 0x00000008

	// NotificationACM codes
	NotificationACMConnectionComplete = 10
	NotificationACMDisconnected       = 21

	// InterfaceState
	InterfaceStateNotReady           = 0
	InterfaceStateConnected          = 1
	InterfaceStateAdHocNetworkFormed = 2
	InterfaceStateDisconnecting      = 3
	InterfaceStateDisconnected       = 4
	InterfaceStateAssociating        = 5
	InterfaceStateDiscovering        = 6
	InterfaceStateAuthenticating     = 7

	// DOT11_SSID
	Dot11SSIDMaxLength = 32
)

type Dot11SSID struct {
	Length uint32
	SSID   [Dot11SSIDMaxLength]byte
}

type Dot11MacAddress [6]byte

type AssociationAttributes struct {
	SSID          Dot11SSID
	BSSType       uint32
	BSSID         Dot11MacAddress
	_             [2]byte // padding for 4-byte alignment
	PhyType       uint32
	PhyIndex      uint32
	SignalQuality uint32
	RxRate        uint32
	TxRate        uint32
}

type SecurityAttributes struct {
	SecurityEnabled int32 // Windows BOOL is 4 bytes
	OneXEnabled     int32
	AuthAlgorithm   uint32
	CipherAlgorithm uint32
}

type ConnectionAttributes struct {
	InterfaceState        uint32
	ConnectionMode        uint32
	ProfileName           [256]uint16
	AssociationAttributes AssociationAttributes
	SecurityAttributes    SecurityAttributes
}

type InterfaceInfo struct {
	InterfaceGUID        windows.GUID
	InterfaceDescription [256]uint16
	InterfaceState       uint32
}

type InterfaceInfoList struct {
	NumberOfItems uint32
	Index         uint32
	InterfaceInfo [1]InterfaceInfo
}

type NotificationData struct {
	NotificationSource uint32
	NotificationCode   uint32
	InterfaceGUID      windows.GUID
	DataSize           uint32
	Data               uintptr
}

// NotificationCallback is the type for notification callback functions.
// Use syscall.NewCallback to create a callback from a Go function.
type NotificationCallback func(data *NotificationData, context uintptr) uintptr

func OpenHandle() (windows.Handle, error) {
	var negotiatedVersion uint32
	var handle windows.Handle
	ret := wlanOpenHandle(ClientVersion2, 0, &negotiatedVersion, &handle)
	if ret != 0 {
		return 0, os.NewSyscallError("WlanOpenHandle", windows.Errno(ret))
	}
	return handle, nil
}

func CloseHandle(handle windows.Handle) error {
	ret := wlanCloseHandle(handle, 0)
	if ret != 0 {
		return os.NewSyscallError("WlanCloseHandle", windows.Errno(ret))
	}
	return nil
}

func EnumInterfaces(handle windows.Handle) ([]InterfaceInfo, error) {
	var list *InterfaceInfoList
	ret := wlanEnumInterfaces(handle, 0, &list)
	if ret != 0 {
		return nil, os.NewSyscallError("WlanEnumInterfaces", windows.Errno(ret))
	}
	defer wlanFreeMemory(uintptr(unsafe.Pointer(list)))

	if list.NumberOfItems == 0 {
		return nil, nil
	}

	interfaces := unsafe.Slice(&list.InterfaceInfo[0], list.NumberOfItems)
	result := make([]InterfaceInfo, list.NumberOfItems)
	copy(result, interfaces)
	return result, nil
}

func QueryCurrentConnection(handle windows.Handle, interfaceGUID *windows.GUID) (*ConnectionAttributes, error) {
	var dataSize uint32
	var data uintptr
	var opcodeValueType uint32

	ret := wlanQueryInterface(handle, interfaceGUID, IntfOpcodeCurrentConnection, 0, &dataSize, &data, &opcodeValueType)
	if ret != 0 {
		return nil, os.NewSyscallError("WlanQueryInterface", windows.Errno(ret))
	}
	defer wlanFreeMemory(data)

	attrs := (*ConnectionAttributes)(unsafe.Pointer(data))
	result := *attrs
	return &result, nil
}

func RegisterNotification(handle windows.Handle, notificationSource uint32, callback uintptr, context uintptr) error {
	var prevSource uint32
	ret := wlanRegisterNotification(handle, notificationSource, false, callback, context, 0, &prevSource)
	if ret != 0 {
		return os.NewSyscallError("WlanRegisterNotification", windows.Errno(ret))
	}
	return nil
}

func UnregisterNotification(handle windows.Handle) error {
	return RegisterNotification(handle, NotificationSourceNone, 0, 0)
}
