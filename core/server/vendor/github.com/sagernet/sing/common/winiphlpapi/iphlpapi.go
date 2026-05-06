//go:build windows

package winiphlpapi

import (
	"errors"
	"os"
	"unsafe"

	"golang.org/x/sys/windows"
)

const (
	TcpTableBasicListener uint32 = iota
	TcpTableBasicConnections
	TcpTableBasicAll
	TcpTableOwnerPidListener
	TcpTableOwnerPidConnections
	TcpTableOwnerPidAll
	TcpTableOwnerModuleListener
	TcpTableOwnerModuleConnections
	TcpTableOwnerModuleAll
)

const (
	UdpTableBasic uint32 = iota
	UdpTableOwnerPid
	UdpTableOwnerModule
)

const (
	TcpConnectionEstatsSynOpts uint32 = iota
	TcpConnectionEstatsData
	TcpConnectionEstatsSndCong
	TcpConnectionEstatsPath
	TcpConnectionEstatsSendBuff
	TcpConnectionEstatsRec
	TcpConnectionEstatsObsRec
	TcpConnectionEstatsBandwidth
	TcpConnectionEstatsFineRtt
	TcpConnectionEstatsMaximum
)

type MibTcpTable struct {
	DwNumEntries uint32
	Table        [1]MibTcpRow
}

type MibTcpRow struct {
	DwState      uint32
	DwLocalAddr  uint32
	DwLocalPort  uint32
	DwRemoteAddr uint32
	DwRemotePort uint32
}

type MibTcp6Table struct {
	DwNumEntries uint32
	Table        [1]MibTcp6Row
}

type MibTcp6Row struct {
	State         uint32
	LocalAddr     [16]byte
	LocalScopeId  uint32
	LocalPort     uint32
	RemoteAddr    [16]byte
	RemoteScopeId uint32
	RemotePort    uint32
}

type MibTcpTableOwnerPid struct {
	DwNumEntries uint32
	Table        [1]MibTcpRowOwnerPid
}

type MibTcpRowOwnerPid struct {
	DwState      uint32
	DwLocalAddr  uint32
	DwLocalPort  uint32
	DwRemoteAddr uint32
	DwRemotePort uint32
	DwOwningPid  uint32
}

type MibTcp6TableOwnerPid struct {
	DwNumEntries uint32
	Table        [1]MibTcp6RowOwnerPid
}

type MibTcp6RowOwnerPid struct {
	UcLocalAddr     [16]byte
	DwLocalScopeId  uint32
	DwLocalPort     uint32
	UcRemoteAddr    [16]byte
	DwRemoteScopeId uint32
	DwRemotePort    uint32
	DwState         uint32
	DwOwningPid     uint32
}

type MibUdpTableOwnerPid struct {
	DwNumEntries uint32
	Table        [1]MibUdpRowOwnerPid
}

type MibUdpRowOwnerPid struct {
	DwLocalAddr uint32
	DwLocalPort uint32
	DwOwningPid uint32
}

type MibUdp6TableOwnerPid struct {
	DwNumEntries uint32
	Table        [1]MibUdp6RowOwnerPid
}

type MibUdp6RowOwnerPid struct {
	UcLocalAddr    [16]byte
	DwLocalScopeId uint32
	DwLocalPort    uint32
	DwOwningPid    uint32
}

type TcpEstatsSendBufferRodV0 struct {
	CurRetxQueue uint64
	MaxRetxQueue uint64
	CurAppWQueue uint64
	MaxAppWQueue uint64
}

type TcpEstatsSendBuffRwV0 struct {
	EnableCollection bool
}

const (
	offsetOfMibTcpTable            = unsafe.Offsetof(MibTcpTable{}.Table)
	offsetOfMibTcp6Table           = unsafe.Offsetof(MibTcp6Table{}.Table)
	offsetOfMibTcpTableOwnerPid    = unsafe.Offsetof(MibTcpTableOwnerPid{}.Table)
	offsetOfMibTcp6TableOwnerPid   = unsafe.Offsetof(MibTcpTableOwnerPid{}.Table)
	offsetOfMibUdpTableOwnerPid    = unsafe.Offsetof(MibUdpTableOwnerPid{}.Table)
	offsetOfMibUdp6TableOwnerPid   = unsafe.Offsetof(MibUdp6TableOwnerPid{}.Table)
	sizeOfTcpEstatsSendBuffRwV0    = unsafe.Sizeof(TcpEstatsSendBuffRwV0{})
	sizeOfTcpEstatsSendBufferRodV0 = unsafe.Sizeof(TcpEstatsSendBufferRodV0{})
)

func GetTcpTable() ([]MibTcpRow, error) {
	var size uint32
	err := getTcpTable(nil, &size, false)
	if !errors.Is(err, windows.ERROR_INSUFFICIENT_BUFFER) {
		return nil, err
	}
	for {
		table := make([]byte, size)
		err = getTcpTable(&table[0], &size, false)
		if err != nil {
			if errors.Is(err, windows.ERROR_INSUFFICIENT_BUFFER) {
				continue
			}
			return nil, err
		}
		dwNumEntries := int(*(*uint32)(unsafe.Pointer(&table[0])))
		return unsafe.Slice((*MibTcpRow)(unsafe.Pointer(&table[offsetOfMibTcpTable])), dwNumEntries), nil
	}
}

func GetTcp6Table() ([]MibTcp6Row, error) {
	var size uint32
	err := getTcp6Table(nil, &size, false)
	if !errors.Is(err, windows.ERROR_INSUFFICIENT_BUFFER) {
		return nil, err
	}
	for {
		table := make([]byte, size)
		err = getTcp6Table(&table[0], &size, false)
		if err != nil {
			if errors.Is(err, windows.ERROR_INSUFFICIENT_BUFFER) {
				continue
			}
			return nil, err
		}
		dwNumEntries := int(*(*uint32)(unsafe.Pointer(&table[0])))
		return unsafe.Slice((*MibTcp6Row)(unsafe.Pointer(&table[offsetOfMibTcp6Table])), dwNumEntries), nil
	}
}

func GetExtendedTcpTable() ([]MibTcpRowOwnerPid, error) {
	var size uint32
	err := getExtendedTcpTable(nil, &size, false, windows.AF_INET, TcpTableOwnerPidConnections, 0)
	if !errors.Is(err, windows.ERROR_INSUFFICIENT_BUFFER) {
		return nil, os.NewSyscallError("GetExtendedTcpTable", err)
	}
	for {
		table := make([]byte, size)
		err = getExtendedTcpTable(&table[0], &size, false, windows.AF_INET, TcpTableOwnerPidConnections, 0)
		if err != nil {
			if errors.Is(err, windows.ERROR_INSUFFICIENT_BUFFER) {
				continue
			}
			return nil, os.NewSyscallError("GetExtendedTcpTable", err)
		}
		dwNumEntries := int(*(*uint32)(unsafe.Pointer(&table[0])))
		return unsafe.Slice((*MibTcpRowOwnerPid)(unsafe.Pointer(&table[offsetOfMibTcpTableOwnerPid])), dwNumEntries), nil
	}
}

func GetExtendedTcp6Table() ([]MibTcp6RowOwnerPid, error) {
	var size uint32
	err := getExtendedTcpTable(nil, &size, false, windows.AF_INET6, TcpTableOwnerPidConnections, 0)
	if !errors.Is(err, windows.ERROR_INSUFFICIENT_BUFFER) {
		return nil, os.NewSyscallError("GetExtendedTcpTable", err)
	}
	for {
		table := make([]byte, size)
		err = getExtendedTcpTable(&table[0], &size, false, windows.AF_INET6, TcpTableOwnerPidConnections, 0)
		if err != nil {
			if errors.Is(err, windows.ERROR_INSUFFICIENT_BUFFER) {
				continue
			}
			return nil, os.NewSyscallError("GetExtendedTcpTable", err)
		}
		dwNumEntries := int(*(*uint32)(unsafe.Pointer(&table[0])))
		return unsafe.Slice((*MibTcp6RowOwnerPid)(unsafe.Pointer(&table[offsetOfMibTcp6TableOwnerPid])), dwNumEntries), nil
	}
}

func GetExtendedUdpTable() ([]MibUdpRowOwnerPid, error) {
	var size uint32
	err := getExtendedUdpTable(nil, &size, false, windows.AF_INET, UdpTableOwnerPid, 0)
	if !errors.Is(err, windows.ERROR_INSUFFICIENT_BUFFER) {
		return nil, os.NewSyscallError("GetExtendedUdpTable", err)
	}
	for {
		table := make([]byte, size)
		err = getExtendedUdpTable(&table[0], &size, false, windows.AF_INET, UdpTableOwnerPid, 0)
		if err != nil {
			if errors.Is(err, windows.ERROR_INSUFFICIENT_BUFFER) {
				continue
			}
			return nil, os.NewSyscallError("GetExtendedUdpTable", err)
		}
		dwNumEntries := int(*(*uint32)(unsafe.Pointer(&table[0])))
		return unsafe.Slice((*MibUdpRowOwnerPid)(unsafe.Pointer(&table[offsetOfMibUdpTableOwnerPid])), dwNumEntries), nil
	}
}

func GetExtendedUdp6Table() ([]MibUdp6RowOwnerPid, error) {
	var size uint32
	err := getExtendedUdpTable(nil, &size, false, windows.AF_INET6, UdpTableOwnerPid, 0)
	if !errors.Is(err, windows.ERROR_INSUFFICIENT_BUFFER) {
		return nil, os.NewSyscallError("GetExtendedUdpTable", err)
	}
	for {
		table := make([]byte, size)
		err = getExtendedUdpTable(&table[0], &size, false, windows.AF_INET6, UdpTableOwnerPid, 0)
		if err != nil {
			if errors.Is(err, windows.ERROR_INSUFFICIENT_BUFFER) {
				continue
			}
			return nil, os.NewSyscallError("GetExtendedUdpTable", err)
		}
		dwNumEntries := int(*(*uint32)(unsafe.Pointer(&table[0])))
		return unsafe.Slice((*MibUdp6RowOwnerPid)(unsafe.Pointer(&table[offsetOfMibUdp6TableOwnerPid])), dwNumEntries), nil
	}
}

func GetPerTcpConnectionEStatsSendBuffer(row *MibTcpRow) (*TcpEstatsSendBufferRodV0, error) {
	var rod TcpEstatsSendBufferRodV0
	err := getPerTcpConnectionEStats(row,
		TcpConnectionEstatsSendBuff,
		0,
		0,
		0,
		0,
		0,
		0,
		uintptr(unsafe.Pointer(&rod)),
		0,
		uint64(sizeOfTcpEstatsSendBufferRodV0),
	)
	if err != nil {
		return nil, err
	}
	return &rod, nil
}

func GetPerTcp6ConnectionEStatsSendBuffer(row *MibTcp6Row) (*TcpEstatsSendBufferRodV0, error) {
	var rod TcpEstatsSendBufferRodV0
	err := getPerTcp6ConnectionEStats(row,
		TcpConnectionEstatsSendBuff,
		0,
		0,
		0,
		0,
		0,
		0,
		uintptr(unsafe.Pointer(&rod)),
		0,
		uint64(sizeOfTcpEstatsSendBufferRodV0),
	)
	if err != nil {
		return nil, err
	}
	return &rod, nil
}

func SetPerTcpConnectionEStatsSendBuffer(row *MibTcpRow, rw *TcpEstatsSendBuffRwV0) error {
	return setPerTcpConnectionEStats(row, TcpConnectionEstatsSendBuff, uintptr(unsafe.Pointer(&rw)), 0, uint64(sizeOfTcpEstatsSendBuffRwV0), 0)
}

func SetPerTcp6ConnectionEStatsSendBuffer(row *MibTcp6Row, rw *TcpEstatsSendBuffRwV0) error {
	return setPerTcp6ConnectionEStats(row, TcpConnectionEstatsSendBuff, uintptr(unsafe.Pointer(&rw)), 0, uint64(sizeOfTcpEstatsSendBuffRwV0), 0)
}
