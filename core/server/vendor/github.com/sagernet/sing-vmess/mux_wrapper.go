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

type MuxConnWrapper struct {
	net.Conn
	writer         N.ExtendedWriter
	destination    M.Socksaddr
	requestWritten bool
	remaining      int
}

func NewMuxConnWrapper(conn net.Conn, destination M.Socksaddr) *MuxConnWrapper {
	return &MuxConnWrapper{
		Conn:        conn,
		writer:      bufio.NewExtendedWriter(conn),
		destination: destination,
	}
}

func (c *MuxConnWrapper) Read(p []byte) (n int, err error) {
	buffer := buf.With(p)
	err = c.ReadBuffer(buffer)
	if err != nil {
		return
	}
	n = buffer.Len()
	return
}

func (c *MuxConnWrapper) Write(p []byte) (n int, err error) {
	return bufio.WriteBuffer(c, buf.As(p))
}

func (c *MuxConnWrapper) ReadBuffer(buffer *buf.Buffer) error {
	if c.remaining > 0 {
		p := buffer.FreeBytes()
		if c.remaining < len(p) {
			p = p[:c.remaining]
		}
		n, err := c.Conn.Read(p)
		if err != nil {
			return err
		}
		c.remaining -= n
		buffer.Truncate(n)
		return nil
	}
	start := buffer.Start()
	_, err := buffer.ReadFullFrom(c.Conn, 6)
	if err != nil {
		return err
	}
	var length uint16
	err = binary.Read(buffer, binary.BigEndian, &length)
	if err != nil {
		return err
	}
	header, err := buffer.ReadBytes(4)
	if err != nil {
		return err
	}

	switch header[2] {
	case StatusNew:
		return E.New("unexpected frame new")
	case StatusKeep:
		if length > 4 {
			_, err = io.CopyN(io.Discard, c.Conn, int64(length-4))
			if err != nil {
				return err
			}
		}
	case StatusEnd:
		return io.EOF
	case StatusKeepAlive:
	default:
		return E.New("unexpected frame: ", buffer.Byte(2))
	}
	// option error
	if header[3]&2 == 2 {
		return E.Cause(net.ErrClosed, "remote closed")
	}
	// option data
	if header[3]&1 != 1 {
		buffer.Resize(start, 0)
		return c.ReadBuffer(buffer)
	} else {
		err = binary.Read(c.Conn, binary.BigEndian, &length)
		if err != nil {
			return err
		}
		c.remaining = int(length)
		buffer.Resize(start, 0)
		return c.ReadBuffer(buffer)
	}
}

func (c *MuxConnWrapper) WriteBuffer(buffer *buf.Buffer) error {
	dataLen := buffer.Len()
	addrLen := M.SocksaddrSerializer.AddrPortLen(c.destination)
	if !c.requestWritten {
		header := buf.With(buffer.ExtendHeader(c.frontHeadroom(addrLen)))
		common.Must(
			binary.Write(header, binary.BigEndian, uint16(5+addrLen)),
			header.WriteByte(0),
			header.WriteByte(0),
			header.WriteByte(1), // frame type new
			header.WriteByte(1), // option data
			header.WriteByte(NetworkTCP),
		)
		err := AddressSerializer.WriteAddrPort(header, c.destination)
		if err != nil {
			return err
		}
		common.Must(binary.Write(header, binary.BigEndian, uint16(dataLen)))
		c.requestWritten = true
	} else {
		header := buffer.ExtendHeader(c.frontHeadroom(addrLen))
		binary.BigEndian.PutUint16(header, 4)
		header[2] = 0
		header[3] = 0
		header[4] = 2 // frame keep
		header[5] = 1 // option data
		binary.BigEndian.PutUint16(header[6:], uint16(dataLen))
	}
	return c.writer.WriteBuffer(buffer)
}

func (c *MuxConnWrapper) frontHeadroom(addrLen int) int {
	if !c.requestWritten {
		var headerLen int
		headerLen += 2 // frame len
		headerLen += 5 // frame header
		headerLen += addrLen
		headerLen += 2 // payload len
		return headerLen
	} else {
		return 8
	}
}

func (c *MuxConnWrapper) FrontHeadroom() int {
	return c.frontHeadroom(M.MaxSocksaddrLength)
}

func (c *MuxConnWrapper) Upstream() any {
	return c.Conn
}
