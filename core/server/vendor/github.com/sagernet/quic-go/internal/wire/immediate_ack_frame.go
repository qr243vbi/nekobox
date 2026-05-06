package wire

import (
	"github.com/sagernet/quic-go/internal/protocol"
	"github.com/sagernet/quic-go/quicvarint"
)

// An ImmediateAckFrame is an IMMEDIATE_ACK frame
type ImmediateAckFrame struct{}

func (f *ImmediateAckFrame) Append(b []byte, _ protocol.Version) ([]byte, error) {
	return quicvarint.Append(b, uint64(FrameTypeImmediateAck)), nil
}

// Length of a written frame
func (f *ImmediateAckFrame) Length(_ protocol.Version) protocol.ByteCount {
	return protocol.ByteCount(quicvarint.Len(uint64(FrameTypeImmediateAck)))
}
