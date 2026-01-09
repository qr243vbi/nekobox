package uot

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

var (
	_ N.NetPacketConn    = (*Conn)(nil)
	_ N.PacketReadWaiter = (*Conn)(nil)
)

type Conn struct {
	net.Conn
	isConnect       bool
	destination     M.Socksaddr
	writer          N.VectorisedWriter
	readWaitOptions N.ReadWaitOptions
}

func NewConn(conn net.Conn, request Request) *Conn {
	uConn := &Conn{
		Conn:        conn,
		isConnect:   request.IsConnect,
		destination: request.Destination,
	}
	uConn.writer, _ = bufio.CreateVectorisedWriter(N.UnwrapWriter(conn))
	return uConn
}

func (c *Conn) Read(p []byte) (n int, err error) {
	n, _, err = c.ReadFrom(p)
	return
}

func (c *Conn) Write(p []byte) (n int, err error) {
	return c.WriteTo(p, c.destination)
}

func (c *Conn) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	var destination M.Socksaddr
	if c.isConnect {
		destination = c.destination
	} else {
		destination, err = AddrParser.ReadAddrPort(c.Conn)
		if err != nil {
			return
		}
	}
	var length uint16
	err = binary.Read(c.Conn, binary.BigEndian, &length)
	if err != nil {
		return
	}
	if len(p) < int(length) {
		err = E.Cause(io.ErrShortBuffer, "UoT read")
		return
	}
	n, err = io.ReadFull(c.Conn, p[:length])
	if err == nil {
		addr = destination.UDPAddr()
	}
	return
}

func (c *Conn) WriteTo(p []byte, addr net.Addr) (n int, err error) {
	destination := M.SocksaddrFromNet(addr)
	var bufferLen int
	if !c.isConnect {
		bufferLen += AddrParser.AddrPortLen(destination)
	}
	bufferLen += 2
	if c.writer == nil {
		bufferLen += len(p)
	}
	buffer := buf.NewSize(bufferLen)
	defer buffer.Release()
	if !c.isConnect {
		err = AddrParser.WriteAddrPort(buffer, destination)
		if err != nil {
			return
		}
	}
	common.Must(binary.Write(buffer, binary.BigEndian, uint16(len(p))))
	if c.writer == nil {
		common.Must1(buffer.Write(p))
		return c.Conn.Write(buffer.Bytes())
	}
	err = c.writer.WriteVectorised([]*buf.Buffer{buffer, buf.As(p)})
	if err == nil {
		n = len(p)
	}
	return
}

func (c *Conn) ReadPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	if c.isConnect {
		destination = c.destination
	} else {
		destination, err = AddrParser.ReadAddrPort(c.Conn)
		if err != nil {
			return
		}
	}
	var length uint16
	err = binary.Read(c.Conn, binary.BigEndian, &length)
	if err != nil {
		return
	}
	_, err = buffer.ReadFullFrom(c.Conn, int(length))
	if err != nil {
		return M.Socksaddr{}, E.Cause(err, "UoT read")
	}
	return
}

func (c *Conn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	var headerLen int
	if !c.isConnect {
		headerLen += AddrParser.AddrPortLen(destination)
	}
	headerLen += 2
	if c.writer == nil {
		headerLen += buffer.Len()
	}
	header := buf.NewSize(headerLen)
	defer header.Release()
	if !c.isConnect {
		err := AddrParser.WriteAddrPort(header, destination)
		if err != nil {
			return err
		}
	}
	common.Must(binary.Write(header, binary.BigEndian, uint16(buffer.Len())))
	if c.writer == nil {
		common.Must1(header.Write(buffer.Bytes()))
		return common.Error(c.Conn.Write(header.Bytes()))
	}
	return c.writer.WriteVectorised([]*buf.Buffer{header, buffer})
}

func (c *Conn) NeedAdditionalReadDeadline() bool {
	return true
}

func (c *Conn) Upstream() any {
	return c.Conn
}
