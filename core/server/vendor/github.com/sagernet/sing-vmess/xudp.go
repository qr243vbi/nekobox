package vmess

import (
	"encoding/binary"
	"io"
	"net"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

type XUDPConn struct {
	net.Conn
	writer         N.ExtendedWriter
	destination    M.Socksaddr
	requestWritten bool
}

func NewXUDPConn(conn net.Conn, destination M.Socksaddr) *XUDPConn {
	return &XUDPConn{
		Conn:        conn,
		writer:      bufio.NewExtendedWriter(conn),
		destination: destination,
	}
}

func (c *XUDPConn) Read(p []byte) (n int, err error) {
	n, _, err = c.ReadFrom(p)
	return
}

func (c *XUDPConn) Write(p []byte) (n int, err error) {
	return c.WriteTo(p, c.destination)
}

func (c *XUDPConn) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	buffer := buf.With(p)
	var destination M.Socksaddr
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

func (c *XUDPConn) ReadPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	start := buffer.Start()
	_, err = buffer.ReadFullFrom(c.Conn, 6)
	if err != nil {
		return
	}
	var length uint16
	err = binary.Read(buffer, binary.BigEndian, &length)
	if err != nil {
		return
	}
	header, err := buffer.ReadBytes(4)
	if err != nil {
		return
	}
	switch header[2] {
	case StatusNew:
		return M.Socksaddr{}, E.New("unexpected frame new")
	case StatusKeep:
		if length != 4 {
			_, err = buffer.ReadFullFrom(c.Conn, int(length)-2)
			if err != nil {
				return
			}
			buffer.Advance(1)
			destination, err = AddressSerializer.ReadAddrPort(buffer)
			if err != nil {
				return
			}
			destination = destination.Unwrap()
		} else {
			_, err = buffer.ReadFullFrom(c.Conn, 2)
			if err != nil {
				return
			}
			destination = c.destination
		}
	case StatusEnd:
		return M.Socksaddr{}, io.EOF
	case StatusKeepAlive:
	default:
		return M.Socksaddr{}, E.New("unexpected frame: ", buffer.Byte(2))
	}
	// option error
	if header[3]&2 == 2 {
		return M.Socksaddr{}, E.Cause(net.ErrClosed, "remote closed")
	}
	// option data
	if header[3]&1 != 1 {
		buffer.Resize(start, 0)
		return c.ReadPacket(buffer)
	} else {
		err = binary.Read(buffer, binary.BigEndian, &length)
		if err != nil {
			return
		}
		buffer.Resize(start, 0)
		_, err = buffer.ReadFullFrom(c.Conn, int(length))
		return
	}
}

func (c *XUDPConn) WriteTo(p []byte, addr net.Addr) (n int, err error) {
	return bufio.WritePacketBuffer(c, buf.As(p), M.SocksaddrFromNet(addr))
}

func (c *XUDPConn) frontHeadroom(addrLen int) int {
	if !c.requestWritten {
		var headerLen int
		headerLen += 2 // frame len
		headerLen += 5 // frame header
		headerLen += addrLen
		headerLen += 2 // payload len
		return headerLen
	} else {
		return 7 + addrLen + 2
	}
}

func (c *XUDPConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	dataLen := buffer.Len()
	addrLen := M.SocksaddrSerializer.AddrPortLen(destination)
	if !c.requestWritten {
		header := buf.With(buffer.ExtendHeader(c.frontHeadroom(addrLen)))
		common.Must(
			binary.Write(header, binary.BigEndian, uint16(5+addrLen)),
			header.WriteByte(0),
			header.WriteByte(0),
			header.WriteByte(1), // frame type new
			header.WriteByte(1), // option data
			header.WriteByte(NetworkUDP),
		)
		err := AddressSerializer.WriteAddrPort(header, destination)
		if err != nil {
			return err
		}
		common.Must(binary.Write(header, binary.BigEndian, uint16(dataLen)))
		c.requestWritten = true
	} else {
		header := buffer.ExtendHeader(c.frontHeadroom(addrLen))
		binary.BigEndian.PutUint16(header, uint16(5+addrLen))
		header[2] = 0
		header[3] = 0
		header[4] = 2 // frame keep
		header[5] = 1 // option data
		header[6] = NetworkUDP
		err := AddressSerializer.WriteAddrPort(buf.With(header[7:]), destination)
		if err != nil {
			return err
		}
		binary.BigEndian.PutUint16(header[7+addrLen:], uint16(dataLen))
	}
	return c.writer.WriteBuffer(buffer)
}

func (c *XUDPConn) FrontHeadroom() int {
	return c.frontHeadroom(M.MaxSocksaddrLength)
}

func (c *XUDPConn) NeedHandshake() bool {
	return !c.requestWritten
}

func (c *XUDPConn) NeedAdditionalReadDeadline() bool {
	return true
}

func (c *XUDPConn) Upstream() any {
	return c.Conn
}
