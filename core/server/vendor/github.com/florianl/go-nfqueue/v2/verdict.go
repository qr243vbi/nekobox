package nfqueue

import (
	"encoding/binary"
	"fmt"

	"github.com/mdlayher/netlink"
)

// VerdictOption configures additional verdict parameters like mark, label, or packet payload.
type VerdictOption func(*verdictOptions) error

type verdictOptions struct {
	attrs   []netlink.Attribute
	ctAttrs []netlink.Attribute
}

// WithMark sets the packet mark.
func WithMark(mark uint32) VerdictOption {
	return func(vo *verdictOptions) error {
		buf := make([]byte, 4)
		binary.BigEndian.PutUint32(buf, mark)
		vo.attrs = append(vo.attrs, netlink.Attribute{
			Type: nfQaMark,
			Data: buf,
		})
		return nil
	}
}

// WithConnMark sets the packet connmark.
func WithConnMark(mark uint32) VerdictOption {
	return func(vo *verdictOptions) error {
		buf := make([]byte, 4)
		binary.BigEndian.PutUint32(buf, mark)
		// collect conntrack attributes; will be nested under nfQaCt later
		vo.ctAttrs = append(vo.ctAttrs, netlink.Attribute{
			Type: ctaMark,
			Data: buf,
		})
		return nil
	}
}

// WithLabel sets the packet label.
func WithLabel(label []byte) VerdictOption {
	return func(vo *verdictOptions) error {
		if len(label) != 16 {
			return fmt.Errorf("conntrack CTA_LABELS must be 16 bytes, got %d", len(label))
		}
		// collect conntrack attributes; will be nested under nfQaCt later
		vo.ctAttrs = append(vo.ctAttrs, netlink.Attribute{
			Type: ctaLabels,
			Data: label,
		})
		return nil
	}
}

// WithAlteredPacket sets the altered packet payload.
func WithAlteredPacket(packet []byte) VerdictOption {
	return func(vo *verdictOptions) error {
		vo.attrs = append(vo.attrs, netlink.Attribute{
			Type: nfQaPayload,
			Data: packet,
		})
		return nil
	}
}

// SetVerdictWithOption signals the kernel the next action for a specified packet id
// and applies any number of verdict options like WithMark, WithLabel, WithPacket.
func (nfqueue *Nfqueue) SetVerdictWithOption(id uint32, verdict int, options ...VerdictOption) error {
	vo := &verdictOptions{}
	for _, opt := range options {
		if err := opt(vo); err != nil {
			return err
		}
	}

	// If conntrack attributes were provided, nest them under nfQaCt
	if len(vo.ctAttrs) > 0 {
		ctData, err := netlink.MarshalAttributes(vo.ctAttrs)
		if err != nil {
			return err
		}
		vo.attrs = append(vo.attrs, netlink.Attribute{
			Type: netlink.Nested | nfQaCt,
			Data: ctData,
		})
	}

	data, err := netlink.MarshalAttributes(vo.attrs)
	if err != nil {
		return err
	}

	return nfqueue.setVerdict(id, verdict, false, data)
}
