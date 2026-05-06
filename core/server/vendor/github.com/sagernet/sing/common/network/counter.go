package network

import (
	"io"
	"syscall"

	"github.com/sagernet/sing/common"
)

type CountFunc func(n int64)

type ReadCounter interface {
	io.Reader
	UnwrapReader() (io.Reader, []CountFunc)
}

type WriteCounter interface {
	io.Writer
	UnwrapWriter() (io.Writer, []CountFunc)
}

type PacketReadCounter interface {
	PacketReader
	UnwrapPacketReader() (PacketReader, []CountFunc)
}

type PacketWriteCounter interface {
	PacketWriter
	UnwrapPacketWriter() (PacketWriter, []CountFunc)
}

func UnwrapCountReader(reader io.Reader, countFunc []CountFunc) (io.Reader, []CountFunc) {
	if counter, isCounter := reader.(ReadCounter); isCounter {
		upstreamReader, upstreamCountFunc := counter.UnwrapReader()
		countFunc = append(countFunc, upstreamCountFunc...)
		return UnwrapCountReader(upstreamReader, countFunc)
	}
	if u, ok := reader.(ReaderWithUpstream); !ok || !u.ReaderReplaceable() {
		return reader, countFunc
	}
	switch u := reader.(type) {
	case ReadWaiter, ReadWaitCreator, syscall.Conn, SyscallReader:
		// In our use cases, counters is always at the top, so we stop when we encounter ReadWaiter
		return reader, countFunc
	case WithUpstreamReader:
		return UnwrapCountReader(u.UpstreamReader().(io.Reader), countFunc)
	case common.WithUpstream:
		return UnwrapCountReader(u.Upstream().(io.Reader), countFunc)
	}
	return reader, countFunc
}

func UnwrapCountWriter(writer io.Writer, countFunc []CountFunc) (io.Writer, []CountFunc) {
	if counter, isCounter := writer.(WriteCounter); isCounter {
		upstreamWriter, upstreamCountFunc := counter.UnwrapWriter()
		countFunc = append(countFunc, upstreamCountFunc...)
		return UnwrapCountWriter(upstreamWriter, countFunc)
	}
	if u, ok := writer.(WriterWithUpstream); !ok || !u.WriterReplaceable() {
		return writer, countFunc
	}
	switch u := writer.(type) {
	case syscall.Conn, SyscallWriter:
		// In our use cases, counters is always at the top, so we stop when we encounter syscall conn
		return writer, countFunc
	case WithUpstreamWriter:
		return UnwrapCountWriter(u.UpstreamWriter().(io.Writer), countFunc)
	case common.WithUpstream:
		return UnwrapCountWriter(u.Upstream().(io.Writer), countFunc)
	}
	return writer, countFunc
}

func UnwrapCountPacketReader(reader PacketReader, countFunc []CountFunc) (PacketReader, []CountFunc) {
	if counter, isCounter := reader.(PacketReadCounter); isCounter {
		upstreamReader, upstreamCountFunc := counter.UnwrapPacketReader()
		countFunc = append(countFunc, upstreamCountFunc...)
		return UnwrapCountPacketReader(upstreamReader, countFunc)
	}
	if u, ok := reader.(ReaderWithUpstream); !ok || !u.ReaderReplaceable() {
		return reader, countFunc
	}
	switch u := reader.(type) {
	case PacketReadWaiter, PacketReadWaitCreator, syscall.Conn:
		// In our use cases, counters is always at the top, so we stop when we encounter ReadWaiter
		return reader, countFunc
	case WithUpstreamReader:
		return UnwrapCountPacketReader(u.UpstreamReader().(PacketReader), countFunc)
	case common.WithUpstream:
		return UnwrapCountPacketReader(u.Upstream().(PacketReader), countFunc)
	}
	return reader, countFunc
}

func UnwrapCountPacketWriter(writer PacketWriter, countFunc []CountFunc) (PacketWriter, []CountFunc) {
	writer = UnwrapPacketWriter(writer)
	if counter, isCounter := writer.(PacketWriteCounter); isCounter {
		upstreamWriter, upstreamCountFunc := counter.UnwrapPacketWriter()
		countFunc = append(countFunc, upstreamCountFunc...)
		return UnwrapCountPacketWriter(upstreamWriter, countFunc)
	}
	if u, ok := writer.(WriterWithUpstream); !ok || !u.WriterReplaceable() {
		return writer, countFunc
	}
	switch u := writer.(type) {
	case syscall.Conn:
		// In our use cases, counters is always at the top, so we stop when we encounter syscall conn
		return writer, countFunc
	case WithUpstreamWriter:
		return UnwrapCountPacketWriter(u.UpstreamWriter().(PacketWriter), countFunc)
	case common.WithUpstream:
		return UnwrapCountPacketWriter(u.Upstream().(PacketWriter), countFunc)
	}
	return writer, countFunc
}
