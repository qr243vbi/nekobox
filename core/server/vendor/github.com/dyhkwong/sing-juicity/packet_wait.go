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

	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/metadata"
	"github.com/sagernet/sing/common/network"
)

var _ network.PacketReadWaiter = (*udpPacketConn)(nil)

func (c *udpPacketConn) InitializeReadWaiter(options network.ReadWaitOptions) (needCopy bool) {
	c.readWaitOptions = options
	return false
}

func (c *udpPacketConn) WaitReadPacket() (buffer *buf.Buffer, destination metadata.Socksaddr, err error) {
	destination, err = metadata.SocksaddrSerializer.ReadAddrPort(c.Conn)
	if err != nil {
		return
	}
	var length uint16
	err = binary.Read(c.Conn, binary.BigEndian, &length)
	if err != nil {
		return
	}
	buffer = c.readWaitOptions.NewPacketBuffer()
	_, err = buffer.ReadFullFrom(c.Conn, int(length))
	if err != nil {
		buffer.Release()
		return
	}
	c.readWaitOptions.PostReturn(buffer)
	return
}
