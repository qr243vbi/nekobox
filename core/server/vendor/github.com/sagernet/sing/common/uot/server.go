package uot

import (
	"encoding/binary"
	"io"
	"net"

	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
)

type ServerConn struct {
	net.PacketConn
	version                   int
	isConnect                 bool
	destination               M.Socksaddr
	inputReader, outputReader *io.PipeReader
	inputWriter, outputWriter *io.PipeWriter
}

func NewServerConn(packetConn net.PacketConn, version int) net.Conn {
	c := &ServerConn{
		PacketConn: packetConn,
		version:    version,
	}
	c.inputReader, c.inputWriter = io.Pipe()
	c.outputReader, c.outputWriter = io.Pipe()
	go c.loopInput()
	go c.loopOutput()
	return c
}

func (c *ServerConn) Read(b []byte) (n int, err error) {
	return c.outputReader.Read(b)
}

func (c *ServerConn) Write(b []byte) (n int, err error) {
	return c.inputWriter.Write(b)
}

func (c *ServerConn) RemoteAddr() net.Addr {
	return M.Socksaddr{Fqdn: "pipe"}
}

//warn:unsafe
func (c *ServerConn) loopInput() {
	if c.version == Version {
		request, err := ReadRequest(c.inputReader)
		if err != nil {
			return
		}
		c.isConnect = request.IsConnect
		c.destination = request.Destination
	}
	buffer := buf.NewPacket()
	defer buffer.Release()
	for {
		var destination M.Socksaddr
		var err error
		if c.isConnect {
			destination = c.destination
		} else {
			destination, err = AddrParser.ReadAddrPort(c.inputReader)
			if err != nil {
				break
			}
		}
		if destination.IsFqdn() {
			addr, err := net.ResolveUDPAddr("udp", destination.String())
			if err != nil {
				continue
			}
			destination = M.SocksaddrFromNet(addr)
		}
		var length uint16
		err = binary.Read(c.inputReader, binary.BigEndian, &length)
		if err != nil {
			break
		}
		buffer.Reset()
		_, err = buffer.ReadFullFrom(c.inputReader, int(length))
		if err != nil {
			break
		}
		_, err = c.WriteTo(buffer.Bytes(), destination.UDPAddr())
		if err != nil {
			break
		}
	}
	c.Close()
}

//warn:unsafe
func (c *ServerConn) loopOutput() {
	buffer := buf.NewPacket()
	defer buffer.Release()
	for {
		buffer.Reset()
		n, addr, err := buffer.ReadPacketFrom(c)
		if err != nil {
			break
		}
		if !c.isConnect {
			err = AddrParser.WriteAddrPort(c.outputWriter, M.SocksaddrFromNet(addr))
			if err != nil {
				break
			}
		}
		err = binary.Write(c.outputWriter, binary.BigEndian, uint16(n))
		if err != nil {
			break
		}
		_, err = buffer.WriteTo(c.outputWriter)
		if err != nil {
			break
		}
	}
	c.Close()
}

func (c *ServerConn) Close() error {
	c.inputReader.Close()
	c.inputWriter.Close()
	c.outputReader.Close()
	c.outputWriter.Close()
	c.PacketConn.Close()
	return nil
}
