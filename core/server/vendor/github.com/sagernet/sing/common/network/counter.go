package network

import (
	"io"
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
	reader = UnwrapReader(reader)
	if counter, isCounter := reader.(ReadCounter); isCounter {
		upstreamReader, upstreamCountFunc := counter.UnwrapReader()
		countFunc = append(countFunc, upstreamCountFunc...)
		return UnwrapCountReader(upstreamReader, countFunc)
	}
	return reader, countFunc
}

func UnwrapCountWriter(writer io.Writer, countFunc []CountFunc) (io.Writer, []CountFunc) {
	writer = UnwrapWriter(writer)
	if counter, isCounter := writer.(WriteCounter); isCounter {
		upstreamWriter, upstreamCountFunc := counter.UnwrapWriter()
		countFunc = append(countFunc, upstreamCountFunc...)
		return UnwrapCountWriter(upstreamWriter, countFunc)
	}
	return writer, countFunc
}

func UnwrapCountPacketReader(reader PacketReader, countFunc []CountFunc) (PacketReader, []CountFunc) {
	reader = UnwrapPacketReader(reader)
	if counter, isCounter := reader.(PacketReadCounter); isCounter {
		upstreamReader, upstreamCountFunc := counter.UnwrapPacketReader()
		countFunc = append(countFunc, upstreamCountFunc...)
		return UnwrapCountPacketReader(upstreamReader, countFunc)
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
	return writer, countFunc
}
