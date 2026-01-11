package bufio

import (
	"io"
	"net"
	"net/netip"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

func NewPacketConn(conn net.PacketConn) N.NetPacketConn {
	if udpConn, isUDPConn := conn.(*net.UDPConn); isUDPConn {
		return &ExtendedUDPConn{udpConn}
	} else if packetConn, isPacketConn := conn.(N.NetPacketConn); isPacketConn && !forceSTDIO {
		return packetConn
	} else {
		return &ExtendedPacketConn{conn}
	}
}

type ExtendedUDPConn struct {
	*net.UDPConn
}

func (w *ExtendedUDPConn) ReadFromUDP(b []byte) (n int, addr *net.UDPAddr, err error) {
	n, addr, err = w.UDPConn.ReadFromUDP(b)
	if err == nil {
		addr.IP = M.AddrFromNet(addr).Unmap().AsSlice()
	}
	return
}

func (w *ExtendedUDPConn) ReadFrom(b []byte) (n int, addr net.Addr, err error) {
	n, addr, err = w.UDPConn.ReadFrom(b)
	if err == nil {
		addr = M.SocksaddrFromNet(addr).Unwrap().UDPAddr()
	}
	return
}

func (w *ExtendedUDPConn) ReadFromUDPAddrPort(b []byte) (n int, addr netip.AddrPort, err error) {
	n, addr, err = w.UDPConn.ReadFromUDPAddrPort(b)
	if err == nil {
		addr = netip.AddrPortFrom(addr.Addr().Unmap(), addr.Port())
	}
	return
}

func (w *ExtendedUDPConn) ReadMsgUDP(b, oob []byte) (n, oobn, flags int, addr *net.UDPAddr, err error) {
	n, oobn, flags, addr, err = w.UDPConn.ReadMsgUDP(b, oob)
	if err == nil {
		addr.IP = M.AddrFromNet(addr).Unmap().AsSlice()
	}
	return
}

func (w *ExtendedUDPConn) ReadMsgUDPAddrPort(b, oob []byte) (n, oobn, flags int, addr netip.AddrPort, err error) {
	n, oobn, flags, addr, err = w.UDPConn.ReadMsgUDPAddrPort(b, oob)
	if err == nil {
		addr = netip.AddrPortFrom(addr.Addr().Unmap(), addr.Port())
	}
	return
}

func (w *ExtendedUDPConn) ReadPacket(buffer *buf.Buffer) (M.Socksaddr, error) {
	n, addr, err := w.ReadFromUDPAddrPort(buffer.FreeBytes())
	if err != nil {
		return M.Socksaddr{}, err
	}
	buffer.Truncate(n)
	return M.SocksaddrFromNetIP(addr).Unwrap(), nil
}

func (w *ExtendedUDPConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	defer buffer.Release()
	return common.Error(w.UDPConn.WriteToUDPAddrPort(buffer.Bytes(), destination.AddrPort()))
}

func (w *ExtendedUDPConn) Upstream() any {
	return w.UDPConn
}

type ExtendedPacketConn struct {
	net.PacketConn
}

func (w *ExtendedPacketConn) ReadFrom(b []byte) (n int, addr net.Addr, err error) {
	n, addr, err = w.PacketConn.ReadFrom(b)
	if err == nil {
		addr = M.SocksaddrFromNet(addr).Unwrap().UDPAddr()
	}
	return
}

func (w *ExtendedPacketConn) ReadPacket(buffer *buf.Buffer) (M.Socksaddr, error) {
	_, addr, err := buffer.ReadPacketFrom(w)
	if err != nil {
		return M.Socksaddr{}, err
	}
	return M.SocksaddrFromNet(addr).Unwrap(), err
}

func (w *ExtendedPacketConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	defer buffer.Release()
	return common.Error(w.WriteTo(buffer.Bytes(), destination.UDPAddr()))
}

func (w *ExtendedPacketConn) Upstream() any {
	return w.PacketConn
}

type ExtendedReaderWrapper struct {
	io.Reader
}

func (r *ExtendedReaderWrapper) ReadBuffer(buffer *buf.Buffer) error {
	n, err := r.Read(buffer.FreeBytes())
	buffer.Truncate(n)
	if n > 0 && err == io.EOF {
		return nil
	}
	return err
}

func (r *ExtendedReaderWrapper) WriteTo(w io.Writer) (n int64, err error) {
	return Copy(w, r.Reader)
}

func (r *ExtendedReaderWrapper) Upstream() any {
	return r.Reader
}

func (r *ExtendedReaderWrapper) ReaderReplaceable() bool {
	return true
}

func NewExtendedReader(reader io.Reader) N.ExtendedReader {
	if forceSTDIO {
		if r, ok := reader.(*ExtendedReaderWrapper); ok {
			return r
		}
	} else {
		if r, ok := reader.(N.ExtendedReader); ok {
			return r
		}
	}
	return &ExtendedReaderWrapper{reader}
}

type ExtendedWriterWrapper struct {
	io.Writer
}

func (w *ExtendedWriterWrapper) WriteBuffer(buffer *buf.Buffer) error {
	defer buffer.Release()
	return common.Error(w.Write(buffer.Bytes()))
}

func (w *ExtendedWriterWrapper) ReadFrom(r io.Reader) (n int64, err error) {
	return Copy(w.Writer, r)
}

func (w *ExtendedWriterWrapper) Upstream() any {
	return w.Writer
}

func (w *ExtendedWriterWrapper) WriterReplaceable() bool {
	return true
}

func NewExtendedWriter(writer io.Writer) N.ExtendedWriter {
	if forceSTDIO {
		if w, ok := writer.(*ExtendedWriterWrapper); ok {
			return w
		}
	} else {
		if w, ok := writer.(N.ExtendedWriter); ok {
			return w
		}
	}
	return &ExtendedWriterWrapper{writer}
}

type ExtendedConnWrapper struct {
	net.Conn
	reader N.ExtendedReader
	writer N.ExtendedWriter
}

func (c *ExtendedConnWrapper) ReadBuffer(buffer *buf.Buffer) error {
	return c.reader.ReadBuffer(buffer)
}

func (c *ExtendedConnWrapper) WriteBuffer(buffer *buf.Buffer) error {
	return c.writer.WriteBuffer(buffer)
}

func (c *ExtendedConnWrapper) ReadFrom(r io.Reader) (n int64, err error) {
	return Copy(c.writer, r)
}

func (c *ExtendedConnWrapper) WriteTo(w io.Writer) (n int64, err error) {
	return Copy(w, c.reader)
}

func (c *ExtendedConnWrapper) UpstreamReader() any {
	return c.reader
}

func (c *ExtendedConnWrapper) ReaderReplaceable() bool {
	return true
}

func (c *ExtendedConnWrapper) UpstreamWriter() any {
	return c.writer
}

func (c *ExtendedConnWrapper) WriterReplaceable() bool {
	return true
}

func (c *ExtendedConnWrapper) Upstream() any {
	return c.Conn
}

func NewExtendedConn(conn net.Conn) N.ExtendedConn {
	if c, ok := conn.(N.ExtendedConn); ok {
		return c
	}
	return &ExtendedConnWrapper{
		Conn:   conn,
		reader: NewExtendedReader(conn),
		writer: NewExtendedWriter(conn),
	}
}
