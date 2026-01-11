package network

import (
	"sync"

	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
)

type PacketBuffer struct {
	Buffer      *buf.Buffer
	Destination M.Socksaddr
}

var packetPool = sync.Pool{
	New: func() any {
		return new(PacketBuffer)
	},
}

func NewPacketBuffer() *PacketBuffer {
	return packetPool.Get().(*PacketBuffer)
}

func PutPacketBuffer(packet *PacketBuffer) {
	*packet = PacketBuffer{}
	packetPool.Put(packet)
}

func ReleaseMultiPacketBuffer(packetBuffers []*PacketBuffer) {
	for _, packet := range packetBuffers {
		packet.Buffer.Release()
		PutPacketBuffer(packet)
	}
}
