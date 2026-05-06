package bufio

import (
	"io"
	"net"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

// Deprecated: bad usage
func ReadBuffer(reader N.ExtendedReader, buffer *buf.Buffer) (n int, err error) {
	n, err = reader.Read(buffer.FreeBytes())
	buffer.Truncate(n)
	return
}

// Deprecated: bad usage
func ReadPacket(reader N.PacketReader, buffer *buf.Buffer) (n int, addr net.Addr, err error) {
	startLen := buffer.Len()
	addr, err = reader.ReadPacket(buffer)
	n = buffer.Len() - startLen
	return
}

func ReadBufferSize(reader io.Reader, bufferSize int) (buffer *buf.Buffer, err error) {
	readWaiter, isReadWaiter := CreateReadWaiter(reader)
	if isReadWaiter {
		readWaiter.InitializeReadWaiter(N.ReadWaitOptions{
			MTU: bufferSize,
		})
		return readWaiter.WaitReadBuffer()
	}
	buffer = buf.NewSize(bufferSize)
	if extendedReader, isExtendedReader := reader.(N.ExtendedReader); isExtendedReader {
		err = extendedReader.ReadBuffer(buffer)
	} else {
		_, err = buffer.ReadOnceFrom(reader)
	}
	if err != nil {
		buffer.Release()
		buffer = nil
	}
	return
}

func ReadPacketSize(reader N.PacketReader, packetSize int) (buffer *buf.Buffer, destination M.Socksaddr, err error) {
	readWaiter, isReadWaiter := CreatePacketReadWaiter(reader)
	if isReadWaiter {
		readWaiter.InitializeReadWaiter(N.ReadWaitOptions{
			MTU: packetSize,
		})
		buffer, destination, err = readWaiter.WaitReadPacket()
		return
	}
	buffer = buf.NewSize(packetSize)
	destination, err = reader.ReadPacket(buffer)
	if err != nil {
		buffer.Release()
		buffer = nil
	}
	return
}

func Write(writer io.Writer, data []byte) (n int, err error) {
	if extendedWriter, isExtended := writer.(N.ExtendedWriter); isExtended {
		return WriteBuffer(extendedWriter, buf.As(data))
	} else {
		return writer.Write(data)
	}
}

func WriteBuffer(writer N.ExtendedWriter, buffer *buf.Buffer) (n int, err error) {
	frontHeadroom := N.CalculateFrontHeadroom(writer)
	rearHeadroom := N.CalculateRearHeadroom(writer)
	if frontHeadroom > buffer.Start() || rearHeadroom > buffer.FreeLen() {
		newBuffer := buf.NewSize(buffer.Len() + frontHeadroom + rearHeadroom)
		newBuffer.Resize(frontHeadroom, 0)
		common.Must1(newBuffer.Write(buffer.Bytes()))
		buffer.Release()
		buffer = newBuffer
	}
	dataLen := buffer.Len()
	err = writer.WriteBuffer(buffer)
	if err == nil {
		n = dataLen
	}
	return
}

func WritePacket(writer N.NetPacketWriter, data []byte, addr net.Addr) (n int, err error) {
	if extendedWriter, isExtended := writer.(N.PacketWriter); isExtended {
		return WritePacketBuffer(extendedWriter, buf.As(data), M.SocksaddrFromNet(addr))
	} else {
		return writer.WriteTo(data, addr)
	}
}

func WritePacketBuffer(writer N.PacketWriter, buffer *buf.Buffer, destination M.Socksaddr) (n int, err error) {
	frontHeadroom := N.CalculateFrontHeadroom(writer)
	rearHeadroom := N.CalculateRearHeadroom(writer)
	if frontHeadroom > buffer.Start() || rearHeadroom > buffer.FreeLen() {
		newBuffer := buf.NewSize(buffer.Len() + frontHeadroom + rearHeadroom)
		newBuffer.Resize(frontHeadroom, 0)
		common.Must1(newBuffer.Write(buffer.Bytes()))
		buffer.Release()
		buffer = newBuffer
	}
	dataLen := buffer.Len()
	err = writer.WritePacket(buffer, destination)
	if err == nil {
		n = dataLen
	}
	return
}

func WriteVectorised(writer N.VectorisedWriter, data [][]byte) (n int, err error) {
	var dataLen int
	buffers := make([]*buf.Buffer, 0, len(data))
	for _, p := range data {
		dataLen += len(p)
		buffers = append(buffers, buf.As(p))
	}
	err = writer.WriteVectorised(buffers)
	if err == nil {
		n = dataLen
	}
	return
}

func WriteVectorisedPacket(writer N.VectorisedPacketWriter, data [][]byte, destination M.Socksaddr) (n int, err error) {
	var dataLen int
	buffers := make([]*buf.Buffer, 0, len(data))
	for _, p := range data {
		dataLen += len(p)
		buffers = append(buffers, buf.As(p))
	}
	err = writer.WriteVectorisedPacket(buffers, destination)
	if err == nil {
		n = dataLen
	}
	return
}
