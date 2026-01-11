//go:build with_gvisor

package tun

import (
	"github.com/sagernet/gvisor/pkg/buffer"
	"github.com/sagernet/gvisor/pkg/tcpip"
	gHdr "github.com/sagernet/gvisor/pkg/tcpip/header"
	"github.com/sagernet/gvisor/pkg/tcpip/link/channel"
	"github.com/sagernet/gvisor/pkg/tcpip/stack"
	"github.com/sagernet/gvisor/pkg/tcpip/transport/udp"
	"github.com/sagernet/sing-tun/internal/gtcpip/header"
	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
)

type Mixed struct {
	*System
	tun      GVisorTun
	stack    *stack.Stack
	endpoint *channel.Endpoint
}

func NewMixed(
	options StackOptions,
) (Stack, error) {
	system, err := NewSystem(options)
	if err != nil {
		return nil, err
	}
	return &Mixed{
		System: system.(*System),
		tun:    system.(*System).tun.(GVisorTun),
	}, nil
}

func (m *Mixed) Start() error {
	err := m.System.start()
	if err != nil {
		return err
	}
	endpoint := channel.New(1024, uint32(m.mtu), "")
	ipStack, err := NewGVisorStack(endpoint)
	if err != nil {
		return err
	}
	ipStack.SetTransportProtocolHandler(udp.ProtocolNumber, NewUDPForwarder(m.ctx, ipStack, m.handler, m.udpTimeout).HandlePacket)
	m.stack = ipStack
	m.endpoint = endpoint
	go m.tunLoop()
	go m.packetLoop()
	return nil
}

func (m *Mixed) Close() error {
	if m.stack == nil {
		return nil
	}
	m.endpoint.Attach(nil)
	m.stack.Close()
	for _, endpoint := range m.stack.CleanupEndpoints() {
		endpoint.Abort()
	}
	return m.System.Close()
}

func (m *Mixed) tunLoop() {
	if winTun, isWinTun := m.tun.(WinTun); isWinTun {
		m.wintunLoop(winTun)
		return
	}
	if linuxTUN, isLinuxTUN := m.tun.(LinuxTUN); isLinuxTUN {
		m.frontHeadroom = linuxTUN.FrontHeadroom()
		m.txChecksumOffload = linuxTUN.TXChecksumOffload()
		batchSize := linuxTUN.BatchSize()
		if batchSize > 1 {
			m.batchLoopLinux(linuxTUN, batchSize)
			return
		}
	}
	if darwinTUN, isDarwinTUN := m.tun.(DarwinTUN); isDarwinTUN && m.multiPendingPackets {
		m.batchLoopDarwin(darwinTUN)
		return
	}
	packetBuffer := make([]byte, m.mtu+PacketOffset)
	for {
		n, err := m.tun.Read(packetBuffer)
		if err != nil {
			if E.IsClosed(err) {
				return
			}
			m.logger.Error(E.Cause(err, "read packet"))
		}
		if n < header.IPv4MinimumSize {
			continue
		}
		rawPacket := packetBuffer[:n]
		packet := packetBuffer[PacketOffset:n]
		if m.processPacket(packet) {
			_, err = m.tun.Write(rawPacket)
			if err != nil {
				m.logger.Trace(E.Cause(err, "write packet"))
			}
		}
	}
}

func (m *Mixed) wintunLoop(winTun WinTun) {
	for {
		packet, release, err := winTun.ReadPacket()
		if err != nil {
			return
		}
		if len(packet) < header.IPv4MinimumSize {
			release()
			continue
		}
		if m.processPacket(packet) {
			_, err = winTun.Write(packet)
			if err != nil {
				m.logger.Trace(E.Cause(err, "write packet"))
			}
		}
		release()
	}
}

func (m *Mixed) batchLoopLinux(linuxTUN LinuxTUN, batchSize int) {
	packetBuffers := make([][]byte, batchSize)
	writeBuffers := make([][]byte, batchSize)
	packetSizes := make([]int, batchSize)
	for i := range packetBuffers {
		packetBuffers[i] = make([]byte, m.mtu+PacketOffset+m.frontHeadroom)
	}
	for {
		n, err := linuxTUN.BatchRead(packetBuffers, m.frontHeadroom, packetSizes)
		if err != nil {
			if E.IsClosed(err) {
				return
			}
			m.logger.Error(E.Cause(err, "batch read packet"))
		}
		if n == 0 {
			continue
		}
		for i := 0; i < n; i++ {
			packetSize := packetSizes[i]
			if packetSize < header.IPv4MinimumSize {
				continue
			}
			packetBuffer := packetBuffers[i]
			packet := packetBuffer[m.frontHeadroom : m.frontHeadroom+packetSize]
			if m.processPacket(packet) {
				writeBuffers = append(writeBuffers, packetBuffer[:m.frontHeadroom+packetSize])
			}
		}
		if len(writeBuffers) > 0 {
			_, err = linuxTUN.BatchWrite(writeBuffers, m.frontHeadroom)
			if err != nil {
				m.logger.Trace(E.Cause(err, "batch write packet"))
			}
			writeBuffers = writeBuffers[:0]
		}
	}
}

func (m *Mixed) batchLoopDarwin(darwinTUN DarwinTUN) {
	var writeBuffers []*buf.Buffer
	for {
		buffers, err := darwinTUN.BatchRead()
		if err != nil {
			if E.IsClosed(err) {
				return
			}
			m.logger.Error(E.Cause(err, "batch read packet"))
		}
		if len(buffers) == 0 {
			continue
		}
		writeBuffers = writeBuffers[:0]
		for _, buffer := range buffers {
			packetSize := buffer.Len()
			if packetSize < header.IPv4MinimumSize {
				continue
			}
			if m.processPacket(buffer.Bytes()) {
				writeBuffers = append(writeBuffers, buffer)
			} else {
				buffer.Release()
			}
		}
		if len(writeBuffers) > 0 {
			err = darwinTUN.BatchWrite(writeBuffers)
			if err != nil {
				m.logger.Trace(E.Cause(err, "batch write packet"))
			}
		}
	}
}

func (m *Mixed) processPacket(packet []byte) bool {
	var (
		writeBack bool
		err       error
	)
	switch ipVersion := header.IPVersion(packet); ipVersion {
	case header.IPv4Version:
		writeBack, err = m.processIPv4(packet)
	case header.IPv6Version:
		writeBack, err = m.processIPv6(packet)
	default:
		err = E.New("ip: unknown version: ", ipVersion)
	}
	if err != nil {
		m.logger.Trace(err)
		return false
	}
	return writeBack
}

func (m *Mixed) processIPv4(ipHdr header.IPv4) (writeBack bool, err error) {
	writeBack = true
	destination := ipHdr.DestinationAddr()
	if destination == m.broadcastAddr || !destination.IsGlobalUnicast() {
		return
	}
	switch ipHdr.TransportProtocol() {
	case header.TCPProtocolNumber:
		writeBack, err = m.processIPv4TCP(ipHdr, ipHdr.Payload())
	case header.UDPProtocolNumber:
		writeBack = false
		pkt := stack.NewPacketBuffer(stack.PacketBufferOptions{
			Payload:           buffer.MakeWithData(ipHdr),
			IsForwardedPacket: true,
		})
		m.endpoint.InjectInbound(gHdr.IPv4ProtocolNumber, pkt)
		pkt.DecRef()
		return
	case header.ICMPv4ProtocolNumber:
		err = m.processIPv4ICMP(ipHdr, ipHdr.Payload())
	}
	return
}

func (m *Mixed) processIPv6(ipHdr header.IPv6) (writeBack bool, err error) {
	writeBack = true
	if !ipHdr.DestinationAddr().IsGlobalUnicast() {
		return
	}
	switch ipHdr.TransportProtocol() {
	case header.TCPProtocolNumber:
		writeBack, err = m.processIPv6TCP(ipHdr, ipHdr.Payload())
	case header.UDPProtocolNumber:
		writeBack = false
		pkt := stack.NewPacketBuffer(stack.PacketBufferOptions{
			Payload:           buffer.MakeWithData(ipHdr),
			IsForwardedPacket: true,
		})
		m.endpoint.InjectInbound(tcpip.NetworkProtocolNumber(header.IPv6ProtocolNumber), pkt)
		pkt.DecRef()
	case header.ICMPv6ProtocolNumber:
		err = m.processIPv6ICMP(ipHdr, ipHdr.Payload())
	}
	return
}

func (m *Mixed) packetLoop() {
	for {
		pkt := m.endpoint.ReadContext(m.ctx)
		if pkt == nil {
			break
		}
		m.tun.WritePacket(pkt)
		pkt.DecRef()
	}
}
