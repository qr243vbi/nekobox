//go:build linux

package tun

import (
	"context"
	"errors"
	"sync/atomic"

	"github.com/sagernet/sing-tun/internal/gtcpip/header"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"

	"github.com/florianl/go-nfqueue/v2"
	"github.com/mdlayher/netlink"
	"golang.org/x/sys/unix"
)

const nfqueueMaxPacketLen = 512

type nfqueueHandler struct {
	ctx        context.Context
	cancel     context.CancelFunc
	handler    Handler
	logger     logger.Logger
	nfq        *nfqueue.Nfqueue
	queue      uint16
	outputMark uint32
	resetMark  uint32
	closed     atomic.Bool
}

type nfqueueOptions struct {
	Context    context.Context
	Handler    Handler
	Logger     logger.Logger
	Queue      uint16
	OutputMark uint32
	ResetMark  uint32
}

func newNFQueueHandler(options nfqueueOptions) (*nfqueueHandler, error) {
	ctx, cancel := context.WithCancel(options.Context)
	return &nfqueueHandler{
		ctx:        ctx,
		cancel:     cancel,
		handler:    options.Handler,
		logger:     options.Logger,
		queue:      options.Queue,
		outputMark: options.OutputMark,
		resetMark:  options.ResetMark,
	}, nil
}

func (h *nfqueueHandler) setVerdict(packetID uint32, verdict int, mark uint32) {
	var err error
	if mark != 0 {
		err = h.nfq.SetVerdictWithOption(packetID, verdict, nfqueue.WithMark(mark))
	} else {
		err = h.nfq.SetVerdict(packetID, verdict)
	}
	if err != nil && !h.closed.Load() && h.ctx.Err() == nil {
		h.logger.Trace(E.Cause(err, "set verdict"))
	}
}

func (h *nfqueueHandler) Start() error {
	config := nfqueue.Config{
		NfQueue:      h.queue,
		MaxPacketLen: nfqueueMaxPacketLen,
		MaxQueueLen:  4096,
		Copymode:     nfqueue.NfQnlCopyPacket,
		AfFamily:     unix.AF_UNSPEC,
		Flags:        nfqueue.NfQaCfgFlagFailOpen,
	}

	nfq, err := nfqueue.Open(&config)
	if err != nil {
		return E.Cause(err, "open nfqueue")
	}

	if err = nfq.SetOption(netlink.NoENOBUFS, true); err != nil {
		nfq.Close()
		return E.Cause(err, "set nfqueue option")
	}

	err = nfq.RegisterWithErrorFunc(h.ctx, h.handlePacket, func(e error) int {
		if h.ctx.Err() != nil {
			return 1
		}
		h.logger.Error("nfqueue error: ", e)
		return 0
	})
	if err != nil {
		nfq.Close()
		return E.Cause(err, "register nfqueue")
	}

	h.nfq = nfq
	return nil
}

func parseIPv6TransportHeader(payload []byte) (transportProto uint8, transportOffset int, ok bool) {
	if len(payload) < header.IPv6MinimumSize {
		return 0, 0, false
	}

	ipv6 := header.IPv6(payload)
	nextHeader := ipv6.NextHeader()
	offset := header.IPv6MinimumSize

	for {
		switch nextHeader {
		case unix.IPPROTO_HOPOPTS,
			unix.IPPROTO_ROUTING,
			unix.IPPROTO_DSTOPTS:
			if len(payload) < offset+2 {
				return 0, 0, false
			}
			nextHeader = payload[offset]
			extLen := int(payload[offset+1]+1) * 8
			if len(payload) < offset+extLen {
				return 0, 0, false
			}
			offset += extLen

		case unix.IPPROTO_FRAGMENT:
			if len(payload) < offset+8 {
				return 0, 0, false
			}
			nextHeader = payload[offset]
			offset += 8

		case unix.IPPROTO_AH:
			if len(payload) < offset+2 {
				return 0, 0, false
			}
			nextHeader = payload[offset]
			extLen := int(payload[offset+1]+2) * 4
			if len(payload) < offset+extLen {
				return 0, 0, false
			}
			offset += extLen

		case unix.IPPROTO_NONE:
			return 0, 0, false

		default:
			return nextHeader, offset, true
		}
	}
}

func (h *nfqueueHandler) handlePacket(attr nfqueue.Attribute) int {
	if h.closed.Load() {
		return 0
	}
	if attr.PacketID == nil || attr.Payload == nil {
		return 0
	}

	packetID := *attr.PacketID
	payload := *attr.Payload

	if len(payload) < header.IPv4MinimumSize {
		h.setVerdict(packetID, nfqueue.NfAccept, 0)
		return 0
	}

	var srcAddr, dstAddr M.Socksaddr
	var tcpOffset int

	version := payload[0] >> 4
	if version == 4 {
		ipv4 := header.IPv4(payload)
		if !ipv4.IsValid(len(payload)) || ipv4.Protocol() != uint8(unix.IPPROTO_TCP) {
			h.setVerdict(packetID, nfqueue.NfAccept, 0)
			return 0
		}
		srcAddr = M.SocksaddrFrom(ipv4.SourceAddr(), 0)
		dstAddr = M.SocksaddrFrom(ipv4.DestinationAddr(), 0)
		tcpOffset = int(ipv4.HeaderLength())
	} else if version == 6 {
		transportProto, transportOffset, ok := parseIPv6TransportHeader(payload)
		if !ok || transportProto != unix.IPPROTO_TCP {
			h.setVerdict(packetID, nfqueue.NfAccept, 0)
			return 0
		}
		ipv6 := header.IPv6(payload)
		srcAddr = M.SocksaddrFrom(ipv6.SourceAddr(), 0)
		dstAddr = M.SocksaddrFrom(ipv6.DestinationAddr(), 0)
		tcpOffset = transportOffset
	} else {
		h.setVerdict(packetID, nfqueue.NfAccept, 0)
		return 0
	}

	if len(payload) < tcpOffset+header.TCPMinimumSize {
		h.setVerdict(packetID, nfqueue.NfAccept, 0)
		return 0
	}

	tcp := header.TCP(payload[tcpOffset:])
	srcAddr = M.SocksaddrFrom(srcAddr.Addr, tcp.SourcePort())
	dstAddr = M.SocksaddrFrom(dstAddr.Addr, tcp.DestinationPort())

	flags := tcp.Flags()
	if !flags.Contains(header.TCPFlagSyn) || flags.Contains(header.TCPFlagAck) {
		h.setVerdict(packetID, nfqueue.NfAccept, 0)
		return 0
	}

	_, pErr := h.handler.PrepareConnection(N.NetworkTCP, srcAddr, dstAddr, nil, 0)

	// Use NfRepeat for bypass/reset so the packet re-enters the chain
	// from the beginning, allowing mark-checking rules to save the mark
	// to conntrack. NfAccept is a terminal verdict in nftables — it exits
	// the chain immediately, skipping any rules after the queue statement.
	switch {
	case errors.Is(pErr, ErrBypass):
		h.setVerdict(packetID, nfqueue.NfRepeat, h.outputMark)
	case errors.Is(pErr, ErrReset):
		h.setVerdict(packetID, nfqueue.NfRepeat, h.resetMark)
	case errors.Is(pErr, ErrDrop):
		h.setVerdict(packetID, nfqueue.NfDrop, 0)
	default:
		h.setVerdict(packetID, nfqueue.NfAccept, 0)
	}

	return 0
}

func (h *nfqueueHandler) Close() error {
	h.closed.Store(true)
	h.cancel()
	if h.nfq != nil {
		h.nfq.Close()
	}
	return nil
}
