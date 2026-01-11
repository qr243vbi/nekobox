package bufio

import (
	"errors"
	"io"
	"syscall"

	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

func copyDirect(source syscall.Conn, destination syscall.Conn, readCounters []N.CountFunc, writeCounters []N.CountFunc) (handed bool, n int64, err error) {
	rawSource, err := source.SyscallConn()
	if err != nil {
		return
	}
	rawDestination, err := destination.SyscallConn()
	if err != nil {
		return
	}
	handed, n, err = splice(rawSource, rawDestination, readCounters, writeCounters)
	return
}

func copyWaitWithPool(originSource io.Reader, destination N.ExtendedWriter, source N.ExtendedReader, readWaiter N.ReadWaiter, options N.ReadWaitOptions, readCounters []N.CountFunc, writeCounters []N.CountFunc, increaseBufferAfter int64) (handled bool, n int64, err error) {
	handled = true
	var (
		buffer       *buf.Buffer
		notFirstTime bool
	)
	for {
		buffer, err = readWaiter.WaitReadBuffer()
		if err != nil {
			if errors.Is(err, io.EOF) {
				err = nil
				return
			}
			return
		}
		dataLen := buffer.Len()
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
			vectorisedReadWaiter, isVectorisedReadWaiter := CreateVectorisedReadWaiter(source)
			vectorisedWriter, isVectorisedWriter := CreateVectorisedWriter(destination)
			if !isVectorisedReadWaiter || !isVectorisedWriter {
				readWaiter.InitializeReadWaiter(options)
				continue
			} else {
				vectorisedReadWaiter.InitializeReadWaiter(options)
			}
			n, err = copyWaitVectorisedWithPool(vectorisedWriter, vectorisedReadWaiter, readCounters, writeCounters, n)
			return
		}
	}
}

func copyWaitVectorisedWithPool(vectorisedWriter N.VectorisedWriter, readWaiter N.VectorisedReadWaiter, readCounters []N.CountFunc, writeCounters []N.CountFunc, inputN int64) (n int64, err error) {
	n += inputN
	var buffers []*buf.Buffer
	for {
		buffers, err = readWaiter.WaitReadBuffers()
		if err != nil {
			if errors.Is(err, io.EOF) {
				err = nil
				return
			}
			return
		}
		var dataLen int
		for _, buffer := range buffers {
			dataLen += buffer.Len()
		}
		err = vectorisedWriter.WriteVectorised(buffers)
		if err != nil {
			for _, buffer := range buffers {
				buffer.Leak()
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
	}
}

func copyPacketWaitWithPool(originSource N.PacketReader, destinationConn N.PacketWriter, source N.PacketReadWaiter, readCounters []N.CountFunc, writeCounters []N.CountFunc, notFirstTime bool) (handled bool, n int64, err error) {
	handled = true
	var (
		buffer      *buf.Buffer
		destination M.Socksaddr
	)
	for {
		buffer, destination, err = source.WaitReadPacket()
		if err != nil {
			return
		}
		dataLen := buffer.Len()
		err = destinationConn.WritePacket(buffer, destination)
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
	}
}
