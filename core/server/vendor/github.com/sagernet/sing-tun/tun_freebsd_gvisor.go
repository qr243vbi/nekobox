//go:build with_gvisor && freebsd

package tun

import (
	"github.com/sagernet/gvisor/pkg/buffer"
	"github.com/sagernet/gvisor/pkg/tcpip"
	"github.com/sagernet/gvisor/pkg/tcpip/header"
	"github.com/sagernet/gvisor/pkg/tcpip/stack"
	"github.com/sagernet/sing/common/bufio"
)

var _ GVisorTun = (*NativeTun)(nil)

func (t *NativeTun) NewEndpoint() (stack.LinkEndpoint, error) {
	return &FreeBSDEndpoint{tun: t}, nil
}

var _ stack.LinkEndpoint = (*FreeBSDEndpoint)(nil)

type FreeBSDEndpoint struct {
	tun        *NativeTun
	dispatcher stack.NetworkDispatcher
}

func (e *FreeBSDEndpoint) Attach(dispatcher stack.NetworkDispatcher) {
	if dispatcher == nil && e.dispatcher != nil {
		e.dispatcher = nil
		return
	}
	if dispatcher != nil && e.dispatcher == nil {
		e.dispatcher = dispatcher
		go e.dispatchLoop()
	}
}

func (e *FreeBSDEndpoint) dispatchLoop() {
	packetBuffer := make([]byte, IFHEADOffset+e.tun.mtu)
	for {
		n, err := e.tun.tunFile.Read(packetBuffer)
		if err != nil {
			break
		}
		// remove IFHEAD here
		packet := packetBuffer[IFHEADOffset:n]
		var networkProtocol tcpip.NetworkProtocolNumber
		switch header.IPVersion(packet) {
		case header.IPv4Version:
			networkProtocol = header.IPv4ProtocolNumber
			if header.IPv4(packet).DestinationAddress().As4() == e.tun.inet4Address {
				e.tun.tunFile.Write(packetBuffer[:n])
				continue
			}
		case header.IPv6Version:
			networkProtocol = header.IPv6ProtocolNumber
			if header.IPv6(packet).DestinationAddress().As16() == e.tun.inet6Address {
				e.tun.tunFile.Write(packetBuffer[:n])
				continue
			}
		default:
			e.tun.tunFile.Write(packetBuffer[:n])
			continue
		}
		pkt := stack.NewPacketBuffer(stack.PacketBufferOptions{
			Payload:           buffer.MakeWithData(packetBuffer[IFHEADOffset:n]),
			IsForwardedPacket: true,
		})
		pkt.NetworkProtocolNumber = networkProtocol
		dispatcher := e.dispatcher
		if dispatcher == nil {
			pkt.DecRef()
			return
		}
		dispatcher.DeliverNetworkPacket(networkProtocol, pkt)
		pkt.DecRef()
	}
}

func (e *FreeBSDEndpoint) IsAttached() bool {
	return e.dispatcher != nil
}

func (e *FreeBSDEndpoint) Wait() {
}

func (e *FreeBSDEndpoint) Capabilities() stack.LinkEndpointCapabilities {
	return stack.CapabilityRXChecksumOffload
}

func (e *FreeBSDEndpoint) ARPHardwareType() header.ARPHardwareType {
	return header.ARPHardwareNone
}

func (e *FreeBSDEndpoint) AddHeader(buffer stack.PacketBufferPtr) {
}

func (e *FreeBSDEndpoint) ParseHeader(ptr stack.PacketBufferPtr) bool {
	return true
}

func (e *FreeBSDEndpoint) MTU() uint32 {
	return e.tun.mtu
}

func (e *FreeBSDEndpoint) MaxHeaderLength() uint16 {
	return 0
}

func (e *FreeBSDEndpoint) LinkAddress() tcpip.LinkAddress {
	return ""
}

func (e *FreeBSDEndpoint) WritePackets(packetBufferList stack.PacketBufferList) (int, tcpip.Error) {
	var n int
	for _, packet := range packetBufferList.AsSlice() {
		_, err := bufio.WriteVectorised(e.tun, packet.AsSlices())
		if err != nil {
			return n, &tcpip.ErrAborted{}
		}
		n++
	}
	return n, nil
}
