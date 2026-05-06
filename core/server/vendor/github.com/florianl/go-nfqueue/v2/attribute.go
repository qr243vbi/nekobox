package nfqueue

import (
	"bytes"
	"encoding/binary"
	"time"

	"github.com/florianl/go-nfqueue/v2/internal/unix"

	"github.com/mdlayher/netlink"
)

func extractAttribute(log Logger, a *Attribute, data []byte) error {
	ad, err := netlink.NewAttributeDecoder(data)
	if err != nil {
		return err
	}
	ad.ByteOrder = binary.BigEndian
	for ad.Next() {
		switch ad.Type() {
		case nfQaPacketHdr:
			packetID := binary.BigEndian.Uint32(ad.Bytes()[:4])
			a.PacketID = &packetID
			hwProto := binary.BigEndian.Uint16(ad.Bytes()[4:6])
			a.HwProtocol = &hwProto
			hook := uint8(ad.Bytes()[6])
			a.Hook = &hook
		case nfQaMark:
			mark := ad.Uint32()
			a.Mark = &mark
		case nfQaTimestamp:
			var sec, usec int64
			r := bytes.NewReader(ad.Bytes()[:8])
			if err := binary.Read(r, binary.BigEndian, &sec); err != nil {
				return err
			}
			r = bytes.NewReader(ad.Bytes()[8:])
			if err := binary.Read(r, binary.BigEndian, &usec); err != nil {
				return err
			}
			timestamp := time.Unix(sec, usec*1000)
			a.Timestamp = &timestamp
		case nfQaIfIndexInDev:
			inDev := ad.Uint32()
			a.InDev = &inDev
		case nfQaIfIndexOutDev:
			outDev := ad.Uint32()
			a.OutDev = &outDev
		case nfQaIfIndexPhysInDev:
			physInDev := ad.Uint32()
			a.PhysInDev = &physInDev
		case nfQaIfIndexPhysOutDev:
			physOutDev := ad.Uint32()
			a.PhysOutDev = &physOutDev
		case nfQaHwAddr:
			hwAddrLen := binary.BigEndian.Uint16(ad.Bytes()[:2])
			hwAddr := (ad.Bytes())[4 : 4+hwAddrLen]
			a.HwAddr = &hwAddr
		case nfQaPayload:
			payload := ad.Bytes()
			a.Payload = &payload
		case nfQaCt:
			ct := ad.Bytes()
			a.Ct = &ct
		case nfQaCtInfo:
			ctInfo := ad.Uint32()
			a.CtInfo = &ctInfo
		case nfQaCapLen:
			capLen := ad.Uint32()
			a.CapLen = &capLen
		case nfQaSkbInfo:
			skbInfo := ad.Bytes()
			a.SkbInfo = &skbInfo
		case nfQaExp:
			exp := ad.Bytes()
			a.Exp = &exp
		case nfQaUID:
			uid := ad.Uint32()
			a.UID = &uid
		case nfQaGID:
			gid := ad.Uint32()
			a.GID = &gid
		case nfQaSecCtx:
			secCtx := ad.String()
			a.SecCtx = &secCtx
		case nfQaL2HDR:
			l2hdr := ad.Bytes()
			a.L2Hdr = &l2hdr
		case nfQaPriority:
			skbPrio := ad.Uint32()
			a.SkbPrio = &skbPrio
		default:
			log.Errorf("Unknown attribute Type: 0x%x\tData: %v", ad.Type(), ad.Bytes())
		}
	}

	return ad.Err()
}

func checkHeader(data []byte) int {
	if (data[0] == unix.AF_INET || data[0] == unix.AF_INET6) && data[1] == unix.NFNETLINK_V0 {
		return 4
	}
	return 0
}

func extractAttributes(log Logger, msg []byte) (Attribute, error) {
	attrs := Attribute{}

	offset := checkHeader(msg[:2])
	if err := extractAttribute(log, &attrs, msg[offset:]); err != nil {
		return attrs, err
	}
	return attrs, nil
}
