package bufio

import (
	"context"
	"errors"
	"io"
	"net"
	"syscall"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/task"
)

const (
	DefaultIncreaseBufferAfter = 512 * 1000
	DefaultBatchSize           = 8
)

func Copy(destination io.Writer, source io.Reader) (n int64, err error) {
	return CopyWithIncreateBuffer(destination, source, DefaultIncreaseBufferAfter, DefaultBatchSize)
}

func CopyWithIncreateBuffer(destination io.Writer, source io.Reader, increaseBufferAfter int64, batchSize int) (n int64, err error) {
	if source == nil {
		return 0, E.New("nil reader")
	} else if destination == nil {
		return 0, E.New("nil writer")
	}
	originSource := source
	var readCounters, writeCounters []N.CountFunc
	for {
		source, readCounters = N.UnwrapCountReader(source, readCounters)
		destination, writeCounters = N.UnwrapCountWriter(destination, writeCounters)
		if cachedSrc, isCached := source.(N.CachedReader); isCached {
			cachedBuffer := cachedSrc.ReadCached()
			if cachedBuffer != nil {
				dataLen := cachedBuffer.Len()
				_, err = destination.Write(cachedBuffer.Bytes())
				cachedBuffer.Release()
				if err != nil {
					return
				}
				for _, counter := range readCounters {
					counter(int64(dataLen))
				}
				for _, counter := range writeCounters {
					counter(int64(dataLen))
				}
				continue
			}
		}
		break
	}
	return CopyWithCounters(destination, source, originSource, readCounters, writeCounters, increaseBufferAfter, batchSize)
}

func CopyWithCounters(destination io.Writer, source io.Reader, originSource io.Reader, readCounters []N.CountFunc, writeCounters []N.CountFunc, increaseBufferAfter int64, batchSize int) (n int64, err error) {
	srcSyscallConn, srcIsSyscall := source.(syscall.Conn)
	dstSyscallConn, dstIsSyscall := destination.(syscall.Conn)
	if srcIsSyscall && dstIsSyscall {
		var handled bool
		handled, n, err = copyDirect(srcSyscallConn, dstSyscallConn, readCounters, writeCounters)
		if handled {
			return
		}
	}
	return CopyExtended(originSource, NewExtendedWriter(destination), NewExtendedReader(source), readCounters, writeCounters, increaseBufferAfter, batchSize)
}

func CopyExtended(originSource io.Reader, destination N.ExtendedWriter, source N.ExtendedReader, readCounters []N.CountFunc, writeCounters []N.CountFunc, increaseBufferAfter int64, batchSize int) (n int64, err error) {
	options := N.NewReadWaitOptions(source, destination)
	options.BatchSize = batchSize
	readWaiter, isReadWaiter := CreateReadWaiter(source)
	if isReadWaiter {
		needCopy := readWaiter.InitializeReadWaiter(options)
		if !needCopy || common.LowMemory {
			var handled bool
			handled, n, err = copyWaitWithPool(originSource, destination, source, readWaiter, options, readCounters, writeCounters, increaseBufferAfter)
			if handled {
				return
			}
		}
	}
	return CopyExtendedWithPool(originSource, destination, source, readCounters, writeCounters, increaseBufferAfter)
}

// Deprecated: not used
func CopyExtendedBuffer(originSource io.Writer, destination N.ExtendedWriter, source N.ExtendedReader, buffer *buf.Buffer, readCounters []N.CountFunc, writeCounters []N.CountFunc) (n int64, err error) {
	buffer.IncRef()
	defer buffer.DecRef()
	frontHeadroom := N.CalculateFrontHeadroom(destination)
	rearHeadroom := N.CalculateRearHeadroom(destination)
	buffer.Resize(frontHeadroom, 0)
	buffer.Reserve(rearHeadroom)
	var notFirstTime bool
	for {
		err = source.ReadBuffer(buffer)
		if err != nil {
			if errors.Is(err, io.EOF) {
				err = nil
				return
			}
			return
		}
		dataLen := buffer.Len()
		buffer.OverCap(rearHeadroom)
		err = destination.WriteBuffer(buffer)
		if err != nil {
			if !notFirstTime {
				err = N.ReportHandshakeFailure(originSource, err)
			}
			return
		}
		n += int64(dataLen)
		for _, counter := range readCounters {
			counter(int64(dataLen))
		}
		for _, counter := range writeCounters {
			counter(int64(dataLen))
		}
		notFirstTime = true
	}
}

func CopyExtendedWithPool(originSource io.Reader, destination N.ExtendedWriter, source N.ExtendedReader, readCounters []N.CountFunc, writeCounters []N.CountFunc, increaseBufferAfter int64) (n int64, err error) {
	options := N.NewReadWaitOptions(source, destination)
	var notFirstTime bool
	for {
		buffer := options.NewBuffer()
		err = source.ReadBuffer(buffer)
		if err != nil {
			buffer.Release()
			if errors.Is(err, io.EOF) {
				err = nil
				return
			}
			return
		}
		dataLen := buffer.Len()
		options.PostReturn(buffer)
		err = destination.WriteBuffer(buffer)
		if err != nil {
			buffer.Leak()
			if !notFirstTime {
				err = N.ReportHandshakeFailure(originSource, err)
			}
			return
		}
		n += int64(dataLen)
		for _, counter := range readCounters {
			counter(int64(dataLen))
		}
		for _, counter := range writeCounters {
			counter(int64(dataLen))
		}
		notFirstTime = true
		if !options.IncreaseBuffer && increaseBufferAfter > 0 && n >= increaseBufferAfter {
			options.IncreaseBuffer = true
		}
	}
}

func CopyConn(ctx context.Context, source net.Conn, destination net.Conn) error {
	var group task.Group
	if _, dstDuplex := common.Cast[N.WriteCloser](destination); dstDuplex {
		group.Append("upload", func(ctx context.Context) error {
			err := common.Error(Copy(destination, source))
			if err == nil {
				N.CloseWrite(destination)
			} else {
				common.Close(destination)
			}
			return err
		})
	} else {
		group.Append("upload", func(ctx context.Context) error {
			defer common.Close(destination)
			return common.Error(Copy(destination, source))
		})
	}
	if _, srcDuplex := common.Cast[N.WriteCloser](source); srcDuplex {
		group.Append("download", func(ctx context.Context) error {
			err := common.Error(Copy(source, destination))
			if err == nil {
				N.CloseWrite(source)
			} else {
				common.Close(source)
			}
			return err
		})
	} else {
		group.Append("download", func(ctx context.Context) error {
			defer common.Close(source)
			return common.Error(Copy(source, destination))
		})
	}
	group.Cleanup(func() {
		common.Close(source, destination)
	})
	return group.Run(ctx)
}

func CopyPacket(destinationConn N.PacketWriter, source N.PacketReader) (n int64, err error) {
	var readCounters, writeCounters []N.CountFunc
	var cachedPackets []*N.PacketBuffer
	originSource := source
	for {
		source, readCounters = N.UnwrapCountPacketReader(source, readCounters)
		destinationConn, writeCounters = N.UnwrapCountPacketWriter(destinationConn, writeCounters)
		if cachedReader, isCached := source.(N.CachedPacketReader); isCached {
			packet := cachedReader.ReadCachedPacket()
			if packet != nil {
				cachedPackets = append(cachedPackets, packet)
				continue
			}
		}
		break
	}
	if cachedPackets != nil {
		n, err = WritePacketWithPool(originSource, destinationConn, cachedPackets, readCounters, writeCounters)
		if err != nil {
			return
		}
	}
	copeN, err := CopyPacketWithCounters(destinationConn, source, originSource, readCounters, writeCounters)
	n += copeN
	return
}

func CopyPacketWithCounters(destinationConn N.PacketWriter, source N.PacketReader, originSource N.PacketReader, readCounters []N.CountFunc, writeCounters []N.CountFunc) (n int64, err error) {
	var (
		handled bool
		copeN   int64
	)
	readWaiter, isReadWaiter := CreatePacketReadWaiter(source)
	if isReadWaiter {
		needCopy := readWaiter.InitializeReadWaiter(N.NewReadWaitOptions(source, destinationConn))
		if !needCopy || common.LowMemory {
			handled, copeN, err = copyPacketWaitWithPool(originSource, destinationConn, readWaiter, readCounters, writeCounters, n > 0)
			if handled {
				n += copeN
				return
			}
		}
	}
	copeN, err = CopyPacketWithPool(originSource, destinationConn, source, readCounters, writeCounters, n > 0)
	n += copeN
	return
}

func CopyPacketWithPool(originSource N.PacketReader, destination N.PacketWriter, source N.PacketReader, readCounters []N.CountFunc, writeCounters []N.CountFunc, notFirstTime bool) (n int64, err error) {
	options := N.NewReadWaitOptions(source, destination)
	var destinationAddress M.Socksaddr
	for {
		buffer := options.NewPacketBuffer()
		destinationAddress, err = source.ReadPacket(buffer)
		if err != nil {
			buffer.Release()
			return
		}
		dataLen := buffer.Len()
		options.PostReturn(buffer)
		err = destination.WritePacket(buffer, destinationAddress)
		if err != nil {
			buffer.Leak()
			if !notFirstTime {
				err = N.ReportHandshakeFailure(originSource, err)
			}
			return
		}
		for _, counter := range readCounters {
			counter(int64(dataLen))
		}
		for _, counter := range writeCounters {
			counter(int64(dataLen))
		}
		n += int64(dataLen)
		notFirstTime = true
	}
}

func WritePacketWithPool(originSource N.PacketReader, destination N.PacketWriter, packetBuffers []*N.PacketBuffer, readCounters []N.CountFunc, writeCounters []N.CountFunc) (n int64, err error) {
	options := N.NewReadWaitOptions(nil, destination)
	var notFirstTime bool
	for _, packetBuffer := range packetBuffers {
		buffer := options.Copy(packetBuffer.Buffer)
		dataLen := buffer.Len()
		err = destination.WritePacket(buffer, packetBuffer.Destination)
		N.PutPacketBuffer(packetBuffer)
		if err != nil {
			buffer.Leak()
			if !notFirstTime {
				err = N.ReportHandshakeFailure(originSource, err)
			}
			return
		}
		for _, counter := range readCounters {
			counter(int64(dataLen))
		}
		for _, counter := range writeCounters {
			counter(int64(dataLen))
		}
		n += int64(dataLen)
		notFirstTime = true
	}
	return
}

func CopyPacketConn(ctx context.Context, source N.PacketConn, destination N.PacketConn) error {
	var group task.Group
	group.Append("upload", func(ctx context.Context) error {
		return common.Error(CopyPacket(destination, source))
	})
	group.Append("download", func(ctx context.Context) error {
		return common.Error(CopyPacket(source, destination))
	})
	group.Cleanup(func() {
		common.Close(source, destination)
	})
	group.FastFail()
	return group.Run(ctx)
}
