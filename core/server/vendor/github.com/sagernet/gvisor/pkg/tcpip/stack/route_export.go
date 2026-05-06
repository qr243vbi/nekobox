package stack

import (
	"github.com/sagernet/gvisor/pkg/tcpip"
)

type exportedEndpoint interface {
	WritePacketDirect(r *Route, pkt *PacketBuffer) tcpip.Error
}

func (r *Route) WritePacketDirect(pkt *PacketBuffer) tcpip.Error {
	rawEndpoint := r.outgoingNIC.getNetworkEndpoint(r.NetProto()).(exportedEndpoint)
	return rawEndpoint.WritePacketDirect(r, pkt)
}
