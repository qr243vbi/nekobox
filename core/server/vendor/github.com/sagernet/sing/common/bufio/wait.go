package bufio

import (
	"io"

	N "github.com/sagernet/sing/common/network"
)

func CreateReadWaiter(reader io.Reader) (N.ReadWaiter, bool) {
	reader = N.UnwrapReader(reader)
	if readWaiter, isReadWaiter := reader.(N.ReadWaiter); isReadWaiter {
		return readWaiter, true
	}
	if readWaitCreator, isCreator := reader.(N.ReadWaitCreator); isCreator {
		return readWaitCreator.CreateReadWaiter()
	}
	if readWaiter, created := createSyscallReadWaiter(reader); created {
		return readWaiter, true
	}
	return nil, false
}

func CreateVectorisedReadWaiter(reader io.Reader) (N.VectorisedReadWaiter, bool) {
	reader = N.UnwrapReader(reader)
	if vectorisedReadWaiter, isVectorised := reader.(N.VectorisedReadWaiter); isVectorised {
		return vectorisedReadWaiter, true
	}
	if readWaitCreator, isCreator := reader.(N.VectorisedReadWaitCreator); isCreator {
		return readWaitCreator.CreateVectorisedReadWaiter()
	}
	if vectorisedReadWaiter, created := createVectorisedSyscallReadWaiter(reader); created {
		return vectorisedReadWaiter, true
	}
	return nil, false
}

func CreatePacketReadWaiter(reader N.PacketReader) (N.PacketReadWaiter, bool) {
	reader = N.UnwrapPacketReader(reader)
	if readWaiter, isReadWaiter := reader.(N.PacketReadWaiter); isReadWaiter {
		return readWaiter, true
	}
	if readWaitCreator, isCreator := reader.(N.PacketReadWaitCreator); isCreator {
		return readWaitCreator.CreateReadWaiter()
	}
	if readWaiter, created := createSyscallPacketReadWaiter(reader); created {
		return readWaiter, true
	}
	return nil, false
}

func CreatePacketVectorisedReadWaiter(reader N.PacketReader) (N.VectorisedPacketReadWaiter, bool) {
	panic("TODO")
}
