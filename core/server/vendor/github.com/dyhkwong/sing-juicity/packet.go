/*
Copyright (C) 2025 dyhkwong

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
*/

package juicity

import (
	"encoding/binary"
	"net"

	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	"github.com/sagernet/sing/common/metadata"
	"github.com/sagernet/sing/common/network"
)

var (
	_ network.NetPacketConn = (*udpPacketConn)(nil)
	_ network.EarlyWriter   = (*udpPacketConn)(nil)
	_ network.FrontHeadroom = (*udpPacketConn)(nil)
)

type udpPacketConn struct {
	net.Conn
	readWaitOptions network.ReadWaitOptions
}

func (c *udpPacketConn) FrontHeadroom() int {
	if clientConn, ok := c.Conn.(*clientConn); ok && !clientConn.requestWritten {
		return 1 + metadata.MaxSocksaddrLength + metadata.MaxSocksaddrLength + 2
	}
	return metadata.MaxSocksaddrLength + 2
}

func (c *udpPacketConn) NeedHandshakeForWrite() bool {
	if clientConn, ok := c.Conn.(*clientConn); ok && !clientConn.requestWritten {
		return true
	}
	return false
}

func (c *udpPacketConn) ReadPacket(buffer *buf.Buffer) (destination metadata.Socksaddr, err error) {
	// The official Juicity server implementation always responses with IPv4-mapped IPv6 address for IPv4, and AddressSerializer.ReadAddrPort has already converted it to the correct one so we don't need to convert it ourselves.
	// This is not documented in Juicity Specification, and this is a bug of the official Juicity server implementation.
	destination, err = AddressSerializer.ReadAddrPort(c.Conn)
	if err != nil {
		return
	}
	var length uint16
	err = binary.Read(c.Conn, binary.BigEndian, &length)
	if err != nil {
		return
	}
	_, err = buffer.ReadFullFrom(c.Conn, int(length))
	return
}

func (c *udpPacketConn) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	buffer := buf.With(p)
	var destination metadata.Socksaddr
	destination, err = c.ReadPacket(buffer)
	if err != nil {
		return
	}
	if destination.IsFqdn() {
		addr = destination
	} else {
		addr = destination.UDPAddr()
	}
	n = buffer.Len()
	return
}

func (c *udpPacketConn) WritePacket(buffer *buf.Buffer, destination metadata.Socksaddr) (err error) {
	defer buffer.Release()
	bufferLen := buffer.Len()
	// The description of UDP header in Juicity Specification is incorrect.
	// The correct one is like: [handshake][address_of_payload0][length_of_payload0][payload0][address_of_payload1][length_of_payload1][payload1]...
	// Where "handshake" is like: [0x03 (Network UDP)][address]. The "address" in handshake should be the same as "address_of_payload0" and it is useless for a bind socket.
	header := buf.With(buffer.ExtendHeader(metadata.SocksaddrSerializer.AddrPortLen(destination) + 2))
	err = metadata.SocksaddrSerializer.WriteAddrPort(header, destination)
	if err != nil {
		return
	}
	err = binary.Write(header, binary.BigEndian, uint16(bufferLen))
	if err != nil {
		return
	}
	_, err = c.Conn.Write(buffer.Bytes())
	return
}

func (c *udpPacketConn) WriteTo(p []byte, addr net.Addr) (n int, err error) {
	return bufio.WritePacketBuffer(c, buf.As(p), metadata.SocksaddrFromNet(addr))
}

func (c *udpPacketConn) Upstream() any {
	return c.Conn
}
