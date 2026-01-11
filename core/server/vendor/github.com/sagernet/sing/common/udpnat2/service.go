package udpnat

import (
	"context"
	"net/netip"
	"time"

	"github.com/sagernet/sing/common"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/pipe"
	"github.com/sagernet/sing/contrab/freelru"
	"github.com/sagernet/sing/contrab/maphash"
)

type Service struct {
	cache   freelru.Cache[netip.AddrPort, *natConn]
	handler N.UDPConnectionHandlerEx
	prepare PrepareFunc
}

type PrepareFunc func(source M.Socksaddr, destination M.Socksaddr, userData any) (bool, context.Context, N.PacketWriter, N.CloseHandlerFunc)

func New(handler N.UDPConnectionHandlerEx, prepare PrepareFunc, timeout time.Duration, shared bool) *Service {
	if timeout == 0 {
		panic("invalid timeout")
	}
	var cache freelru.Cache[netip.AddrPort, *natConn]
	if !shared {
		cache = common.Must1(freelru.NewSynced[netip.AddrPort, *natConn](1024, maphash.NewHasher[netip.AddrPort]().Hash32))
	} else {
		cache = common.Must1(freelru.NewSharded[netip.AddrPort, *natConn](1024, maphash.NewHasher[netip.AddrPort]().Hash32))
	}
	cache.SetLifetime(timeout)
	cache.SetHealthCheck(func(port netip.AddrPort, conn *natConn) bool {
		select {
		case <-conn.doneChan:
			return false
		default:
			return true
		}
	})
	cache.SetOnEvict(func(_ netip.AddrPort, conn *natConn) {
		conn.Close()
	})
	return &Service{
		cache:   cache,
		handler: handler,
		prepare: prepare,
	}
}

func (s *Service) NewPacket(bufferSlices [][]byte, source M.Socksaddr, destination M.Socksaddr, userData any) {
	conn, _, ok := s.cache.GetAndRefreshOrAdd(source.AddrPort(), func() (*natConn, bool) {
		ok, ctx, writer, onClose := s.prepare(source, destination, userData)
		if !ok {
			return nil, false
		}
		newConn := &natConn{
			cache:        s.cache,
			writer:       writer,
			localAddr:    source,
			packetChan:   make(chan *N.PacketBuffer, 64),
			doneChan:     make(chan struct{}),
			readDeadline: pipe.MakeDeadline(),
		}
		go s.handler.NewPacketConnectionEx(ctx, newConn, source, destination, onClose)
		return newConn, true
	})
	if !ok {
		return
	}
	conn.handlerAccess.RLock()
	readWaitOptions := conn.readWaitOptions
	handler := conn.handler
	conn.handlerAccess.RUnlock()
	buffer := readWaitOptions.NewPacketBuffer()
	for _, bufferSlice := range bufferSlices {
		buffer.Write(bufferSlice)
	}
	if handler != nil {
		handler.NewPacketEx(buffer, destination)
		return
	}
	packet := N.NewPacketBuffer()
	*packet = N.PacketBuffer{
		Buffer:      buffer,
		Destination: destination,
	}
	select {
	case conn.packetChan <- packet:
	default:
		packet.Buffer.Release()
		N.PutPacketBuffer(packet)
	}
}

func (s *Service) Purge() {
	s.cache.Purge()
}

func (s *Service) PurgeExpired() {
	s.cache.PurgeExpired()
}
