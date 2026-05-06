package winiphlpapi

//go:generate go run golang.org/x/sys/windows/mkwinsyscall -output zsyscall_windows.go syscall_windows.go

// https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-gettcptable
//sys getTcpTable(tcpTable *byte, sizePointer *uint32, order bool) (errcode error) = iphlpapi.GetTcpTable

// https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-gettcp6table
//sys getTcp6Table(tcpTable *byte, sizePointer *uint32, order bool) (errcode error) = iphlpapi.GetTcp6Table

// https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getpertcpconnectionestats
//sys getPerTcpConnectionEStats(row *MibTcpRow, estatsType uint32, rw uintptr, rwVersion uint64, rwSize uint64, ros uintptr, rosVersion uint64, rosSize uint64, rod uintptr, rodVersion uint64, rodSize uint64) (errcode error) = iphlpapi.GetPerTcpConnectionEStats

// https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getpertcp6connectionestats
//sys getPerTcp6ConnectionEStats(row *MibTcp6Row, estatsType uint32, rw uintptr, rwVersion uint64, rwSize uint64, ros uintptr, rosVersion uint64, rosSize uint64, rod uintptr, rodVersion uint64, rodSize uint64) (errcode error) = iphlpapi.GetPerTcp6ConnectionEStats

// https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-setpertcpconnectionestats
//sys setPerTcpConnectionEStats(row *MibTcpRow, estatsType uint32, rw uintptr, rwVersion uint64, rwSize uint64, offset uint64) (errcode error) = iphlpapi.SetPerTcpConnectionEStats

// https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-setpertcp6connectionestats
//sys setPerTcp6ConnectionEStats(row *MibTcp6Row, estatsType uint32, rw uintptr, rwVersion uint64, rwSize uint64, offset uint64) (errcode error) = iphlpapi.SetPerTcp6ConnectionEStats

// https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getextendedtcptable
//sys getExtendedTcpTable(pTcpTable *byte, pdwSize *uint32, bOrder bool, ulAf uint64, tableClass uint32, reserved uint64) = (errcode error) = iphlpapi.GetExtendedTcpTable

// https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getextendedudptable
//sys getExtendedUdpTable(pUdpTable *byte, pdwSize *uint32, bOrder bool, ulAf uint64, tableClass uint32, reserved uint64) = (errcode error) = iphlpapi.GetExtendedUdpTable
