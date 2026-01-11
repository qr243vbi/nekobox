//go:build windows

package winiphlpapi

import (
	"context"
	"encoding/binary"
	"net"
	"net/netip"
	"os"
	"runtime"
	"time"
	"unsafe"

	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

func LoadEStats() error {
	err := modiphlpapi.Load()
	if err != nil {
		return err
	}
	err = procGetTcpTable.Find()
	if err != nil {
		return err
	}
	err = procGetTcp6Table.Find()
	if err != nil {
		return err
	}
	err = procGetPerTcp6ConnectionEStats.Find()
	if err != nil {
		return err
	}
	err = procGetPerTcp6ConnectionEStats.Find()
	if err != nil {
		return err
	}
	err = procSetPerTcpConnectionEStats.Find()
	if err != nil {
		return err
	}
	err = procSetPerTcp6ConnectionEStats.Find()
	if err != nil {
		return err
	}
	return nil
}

func LoadExtendedTable() error {
	err := modiphlpapi.Load()
	if err != nil {
		return err
	}
	err = procGetExtendedTcpTable.Find()
	if err != nil {
		return err
	}
	err = procGetExtendedUdpTable.Find()
	if err != nil {
		return err
	}
	return nil
}

func FindPid(network string, source netip.AddrPort) (uint32, error) {
	switch N.NetworkName(network) {
	case N.NetworkTCP:
		if source.Addr().Is4() {
			tcpTable, err := GetExtendedTcpTable()
			if err != nil {
				return 0, err
			}
			for _, row := range tcpTable {
				if source == netip.AddrPortFrom(DwordToAddr(row.DwLocalAddr), DwordToPort(row.DwLocalPort)) {
					return row.DwOwningPid, nil
				}
			}
		} else {
			tcpTable, err := GetExtendedTcp6Table()
			if err != nil {
				return 0, err
			}
			for _, row := range tcpTable {
				if source == netip.AddrPortFrom(netip.AddrFrom16(row.UcLocalAddr), DwordToPort(row.DwLocalPort)) {
					return row.DwOwningPid, nil
				}
			}
		}
	case N.NetworkUDP:
		if source.Addr().Is4() {
			udpTable, err := GetExtendedUdpTable()
			if err != nil {
				return 0, err
			}
			for _, row := range udpTable {
				if source == netip.AddrPortFrom(DwordToAddr(row.DwLocalAddr), DwordToPort(row.DwLocalPort)) ||
					runtime.GOOS == "windows" && DwordToAddr(row.DwLocalAddr) == netip.IPv4Unspecified() && source.Port() == DwordToPort(row.DwLocalPort) {
					return row.DwOwningPid, nil
				}
			}
		} else {
			udpTable, err := GetExtendedUdp6Table()
			if err != nil {
				return 0, err
			}
			for _, row := range udpTable {
				if source == netip.AddrPortFrom(netip.AddrFrom16(row.UcLocalAddr), DwordToPort(row.DwLocalPort)) ||
					runtime.GOOS == "windows" && netip.AddrFrom16(row.UcLocalAddr) == netip.IPv6Unspecified() && source.Port() == DwordToPort(row.DwLocalPort) {
					return row.DwOwningPid, nil
				}
			}
		}
	}
	return 0, E.New("process not found for ", source)
}

func WriteAndWaitAck(ctx context.Context, conn net.Conn, payload []byte) error {
	source := M.AddrPortFromNet(conn.LocalAddr())
	destination := M.AddrPortFromNet(conn.RemoteAddr())
	if source.Addr().Is4() {
		tcpTable, err := GetTcpTable()
		if err != nil {
			return err
		}
		var tcpRow *MibTcpRow
		for _, row := range tcpTable {
			if source == netip.AddrPortFrom(DwordToAddr(row.DwLocalAddr), DwordToPort(row.DwLocalPort)) ||
				destination == netip.AddrPortFrom(DwordToAddr(row.DwRemoteAddr), DwordToPort(row.DwRemotePort)) {
				tcpRow = &row
				break
			}
		}
		if tcpRow == nil {
			return E.New("row not found for: ", source)
		}
		err = SetPerTcpConnectionEStatsSendBuffer(tcpRow, &TcpEstatsSendBuffRwV0{
			EnableCollection: true,
		})
		if err != nil {
			return os.NewSyscallError("SetPerTcpConnectionEStatsSendBufferV0", err)
		}
		defer SetPerTcpConnectionEStatsSendBuffer(tcpRow, &TcpEstatsSendBuffRwV0{
			EnableCollection: false,
		})
		_, err = conn.Write(payload)
		if err != nil {
			return err
		}
		for {
			select {
			case <-ctx.Done():
				return ctx.Err()
			default:
			}
			eStstsSendBuffer, err := GetPerTcpConnectionEStatsSendBuffer(tcpRow)
			if err != nil {
				return err
			}
			if eStstsSendBuffer.CurRetxQueue == 0 {
				return nil
			}
			time.Sleep(10 * time.Millisecond)
		}
	} else {
		tcpTable, err := GetTcp6Table()
		if err != nil {
			return err
		}
		var tcpRow *MibTcp6Row
		for _, row := range tcpTable {
			if source == netip.AddrPortFrom(netip.AddrFrom16(row.LocalAddr), DwordToPort(row.LocalPort)) ||
				destination == netip.AddrPortFrom(netip.AddrFrom16(row.RemoteAddr), DwordToPort(row.RemotePort)) {
				tcpRow = &row
				break
			}
		}
		if tcpRow == nil {
			return E.New("row not found for: ", source)
		}
		err = SetPerTcp6ConnectionEStatsSendBuffer(tcpRow, &TcpEstatsSendBuffRwV0{
			EnableCollection: true,
		})
		if err != nil {
			return os.NewSyscallError("SetPerTcpConnectionEStatsSendBufferV0", err)
		}
		defer SetPerTcp6ConnectionEStatsSendBuffer(tcpRow, &TcpEstatsSendBuffRwV0{
			EnableCollection: false,
		})
		_, err = conn.Write(payload)
		if err != nil {
			return err
		}
		for {
			select {
			case <-ctx.Done():
				return ctx.Err()
			default:
			}
			eStstsSendBuffer, err := GetPerTcp6ConnectionEStatsSendBuffer(tcpRow)
			if err != nil {
				return err
			}
			if eStstsSendBuffer.CurRetxQueue == 0 {
				return nil
			}
			time.Sleep(10 * time.Millisecond)
		}
	}
}

func DwordToAddr(addr uint32) netip.Addr {
	return netip.AddrFrom4(*(*[4]byte)(unsafe.Pointer(&addr)))
}

func DwordToPort(dword uint32) uint16 {
	return binary.BigEndian.Uint16((*[4]byte)(unsafe.Pointer(&dword))[:])
}
