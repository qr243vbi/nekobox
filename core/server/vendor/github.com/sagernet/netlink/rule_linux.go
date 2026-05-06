package netlink

import (
	"bytes"
	"fmt"
	"net/netip"

	"github.com/sagernet/netlink/nl"
	"golang.org/x/sys/unix"
)

const FibRuleInvert = 0x2

// RuleAdd adds a rule to the system.
// Equivalent to: ip rule add
func RuleAdd(rule *Rule) error {
	return pkgHandle.RuleAdd(rule)
}

// RuleAdd adds a rule to the system.
// Equivalent to: ip rule add
func (h *Handle) RuleAdd(rule *Rule) error {
	req := h.newNetlinkRequest(unix.RTM_NEWRULE, unix.NLM_F_CREATE|unix.NLM_F_EXCL|unix.NLM_F_ACK)
	return ruleHandle(rule, req)
}

// RuleDel deletes a rule from the system.
// Equivalent to: ip rule del
func RuleDel(rule *Rule) error {
	return pkgHandle.RuleDel(rule)
}

// RuleDel deletes a rule from the system.
// Equivalent to: ip rule del
func (h *Handle) RuleDel(rule *Rule) error {
	req := h.newNetlinkRequest(unix.RTM_DELRULE, unix.NLM_F_ACK)
	return ruleHandle(rule, req)
}

func ruleHandle(rule *Rule, req *nl.NetlinkRequest) error {
	msg := nl.NewRtMsg()
	msg.Family = unix.AF_UNSPEC
	msg.Protocol = unix.RTPROT_BOOT
	msg.Scope = unix.RT_SCOPE_UNIVERSE
	msg.Table = unix.RT_TABLE_UNSPEC
	msg.Type = unix.RTN_UNSPEC
	if rule.Type > 0 {
		msg.Type = rule.Type
	} else if rule.Table >= 0 {
		msg.Type = unix.FR_ACT_TO_TBL
	} else if rule.Goto >= 0 {
		msg.Type = unix.FR_ACT_GOTO
	} else if req.NlMsghdr.Flags&unix.NLM_F_CREATE > 0 {
		msg.Type = unix.FR_ACT_NOP
	}
	if rule.Invert {
		msg.Flags |= FibRuleInvert
	}
	if rule.Family != 0 {
		msg.Family = uint8(rule.Family)
	}
	if rule.Table >= 0 && rule.Table < 256 {
		msg.Table = uint8(rule.Table)
	}
	if rule.Tos != 0 {
		msg.Tos = uint8(rule.Tos)
	}

	var dstFamily uint8
	var rtAttrs []*nl.RtAttr

	if rule.Dst.IsValid() {
		msg.Dst_len = uint8(rule.Dst.Bits())
		msg.Family = uint8(nl.GetIPFamily(rule.Dst.Addr().AsSlice()))
		dstFamily = msg.Family
		var dstData []byte
		if msg.Family == unix.AF_INET {
			dstData = netip.AddrFrom4(rule.Dst.Addr().As4()).AsSlice()
		} else {
			dstData = rule.Dst.Addr().AsSlice()
		}
		rtAttrs = append(rtAttrs, nl.NewRtAttr(unix.RTA_DST, dstData))
	}

	if rule.Src.IsValid() {
		msg.Src_len = uint8(rule.Src.Bits())
		msg.Family = uint8(nl.GetIPFamily(rule.Src.Addr().AsSlice()))
		if dstFamily != 0 && dstFamily != msg.Family {
			return fmt.Errorf("source and destination ip are not the same IP family")
		}
		var srcData []byte
		if msg.Family == unix.AF_INET {
			srcData = netip.AddrFrom4(rule.Src.Addr().As4()).AsSlice()
		} else {
			srcData = rule.Src.Addr().AsSlice()
		}
		rtAttrs = append(rtAttrs, nl.NewRtAttr(unix.RTA_SRC, srcData))
	}

	req.AddData(msg)
	for i := range rtAttrs {
		req.AddData(rtAttrs[i])
	}

	if rule.Priority >= 0 {
		b := make([]byte, 4)
		native.PutUint32(b, uint32(rule.Priority))
		req.AddData(nl.NewRtAttr(nl.FRA_PRIORITY, b))
	}
	if rule.MarkSet {
		b := make([]byte, 4)
		native.PutUint32(b, rule.Mark)
		req.AddData(nl.NewRtAttr(nl.FRA_FWMARK, b))
	}
	if rule.Mask >= 0 {
		b := make([]byte, 4)
		native.PutUint32(b, uint32(rule.Mask))
		req.AddData(nl.NewRtAttr(nl.FRA_FWMASK, b))
	}
	if rule.Flow >= 0 {
		b := make([]byte, 4)
		native.PutUint32(b, uint32(rule.Flow))
		req.AddData(nl.NewRtAttr(nl.FRA_FLOW, b))
	}
	if rule.TunID > 0 {
		b := make([]byte, 4)
		native.PutUint32(b, uint32(rule.TunID))
		req.AddData(nl.NewRtAttr(nl.FRA_TUN_ID, b))
	}
	if rule.Table >= 0 {
		b := make([]byte, 4)
		native.PutUint32(b, uint32(rule.Table))
		req.AddData(nl.NewRtAttr(nl.FRA_TABLE, b))
		if rule.SuppressPrefixlen >= 0 {
			b := make([]byte, 4)
			native.PutUint32(b, uint32(rule.SuppressPrefixlen))
			req.AddData(nl.NewRtAttr(nl.FRA_SUPPRESS_PREFIXLEN, b))
		}
		if rule.SuppressIfgroup >= 0 {
			b := make([]byte, 4)
			native.PutUint32(b, uint32(rule.SuppressIfgroup))
			req.AddData(nl.NewRtAttr(nl.FRA_SUPPRESS_IFGROUP, b))
		}
	}
	if rule.IifName != "" {
		req.AddData(nl.NewRtAttr(nl.FRA_IIFNAME, []byte(rule.IifName+"\x00")))
	}
	if rule.OifName != "" {
		req.AddData(nl.NewRtAttr(nl.FRA_OIFNAME, []byte(rule.OifName+"\x00")))
	}
	if rule.Goto >= 0 {
		b := make([]byte, 4)
		native.PutUint32(b, uint32(rule.Goto))
		req.AddData(nl.NewRtAttr(nl.FRA_GOTO, b))
	}

	if rule.IPProto > 0 {
		b := make([]byte, 4)
		native.PutUint32(b, uint32(rule.IPProto))
		req.AddData(nl.NewRtAttr(nl.FRA_IP_PROTO, b))
	}

	if rule.Dport != nil {
		b := rule.Dport.toRtAttrData()
		req.AddData(nl.NewRtAttr(nl.FRA_DPORT_RANGE, b))
	}

	if rule.Sport != nil {
		b := rule.Sport.toRtAttrData()
		req.AddData(nl.NewRtAttr(nl.FRA_SPORT_RANGE, b))
	}

	if rule.UIDRange != nil {
		b := rule.UIDRange.toRtAttrData()
		req.AddData(nl.NewRtAttr(nl.FRA_UID_RANGE, b))
	}

	_, err := req.Execute(unix.NETLINK_ROUTE, 0)
	return err
}

// RuleList lists rules in the system.
// Equivalent to: ip rule list
func RuleList(family int) ([]Rule, error) {
	return pkgHandle.RuleList(family)
}

// RuleList lists rules in the system.
// Equivalent to: ip rule list
func (h *Handle) RuleList(family int) ([]Rule, error) {
	return h.RuleListFiltered(family, nil, 0)
}

// RuleListFiltered gets a list of rules in the system filtered by the
// specified rule template `filter`.
// Equivalent to: ip rule list
func RuleListFiltered(family int, filter *Rule, filterMask uint64) ([]Rule, error) {
	return pkgHandle.RuleListFiltered(family, filter, filterMask)
}

// RuleListFiltered lists rules in the system.
// Equivalent to: ip rule list
func (h *Handle) RuleListFiltered(family int, filter *Rule, filterMask uint64) ([]Rule, error) {
	req := h.newNetlinkRequest(unix.RTM_GETRULE, unix.NLM_F_DUMP|unix.NLM_F_REQUEST)
	msg := nl.NewIfInfomsg(family)
	req.AddData(msg)

	msgs, err := req.Execute(unix.NETLINK_ROUTE, unix.RTM_NEWRULE)
	if err != nil {
		return nil, err
	}

	var res = make([]Rule, 0)
	for i := range msgs {
		msg := nl.DeserializeRtMsg(msgs[i])
		attrs, err := nl.ParseRouteAttr(msgs[i][msg.Len():])
		if err != nil {
			return nil, err
		}

		rule := NewRule()
		rule.Family = int(msg.Family)
		rule.Invert = msg.Flags&FibRuleInvert > 0
		rule.Tos = uint(msg.Tos)

		for j := range attrs {
			switch attrs[j].Attr.Type {
			case unix.RTA_TABLE:
				rule.Table = int(native.Uint32(attrs[j].Value[0:4]))
			case nl.FRA_SRC:
				addr, _ := netip.AddrFromSlice(attrs[j].Value)
				if addr.Is4In6() {
					addr = netip.AddrFrom4(addr.As4())
				}
				rule.Src = netip.PrefixFrom(addr, int(msg.Src_len))
			case nl.FRA_DST:
				addr, _ := netip.AddrFromSlice(attrs[j].Value)
				if addr.Is4In6() {
					addr = netip.AddrFrom4(addr.As4())
				}
				rule.Dst = netip.PrefixFrom(addr, int(msg.Dst_len))
			case nl.FRA_FWMARK:
				rule.Mark = native.Uint32(attrs[j].Value[0:4])
			case nl.FRA_FWMASK:
				rule.Mask = int(native.Uint32(attrs[j].Value[0:4]))
			case nl.FRA_TUN_ID:
				rule.TunID = uint(native.Uint64(attrs[j].Value[0:8]))
			case nl.FRA_IIFNAME:
				rule.IifName = string(attrs[j].Value[:len(attrs[j].Value)-1])
			case nl.FRA_OIFNAME:
				rule.OifName = string(attrs[j].Value[:len(attrs[j].Value)-1])
			case nl.FRA_SUPPRESS_PREFIXLEN:
				i := native.Uint32(attrs[j].Value[0:4])
				if i != 0xffffffff {
					rule.SuppressPrefixlen = int(i)
				}
			case nl.FRA_SUPPRESS_IFGROUP:
				i := native.Uint32(attrs[j].Value[0:4])
				if i != 0xffffffff {
					rule.SuppressIfgroup = int(i)
				}
			case nl.FRA_FLOW:
				rule.Flow = int(native.Uint32(attrs[j].Value[0:4]))
			case nl.FRA_GOTO:
				rule.Goto = int(native.Uint32(attrs[j].Value[0:4]))
			case nl.FRA_PRIORITY:
				rule.Priority = int(native.Uint32(attrs[j].Value[0:4]))
			case nl.FRA_IP_PROTO:
				rule.IPProto = int(native.Uint32(attrs[j].Value[0:4]))
			case nl.FRA_DPORT_RANGE:
				rule.Dport = NewRulePortRange(native.Uint16(attrs[j].Value[0:2]), native.Uint16(attrs[j].Value[2:4]))
			case nl.FRA_SPORT_RANGE:
				rule.Sport = NewRulePortRange(native.Uint16(attrs[j].Value[0:2]), native.Uint16(attrs[j].Value[2:4]))
			case nl.FRA_UID_RANGE:
				rule.UIDRange = NewRuleUIDRange(native.Uint32(attrs[j].Value[0:4]), native.Uint32(attrs[j].Value[4:8]))
			}
		}

		if filter != nil {
			switch {
			case filterMask&RT_FILTER_SRC != 0 &&
				(rule.Src.IsValid() || rule.Src.String() != filter.Src.String()):
				continue
			case filterMask&RT_FILTER_DST != 0 &&
				(rule.Dst.IsValid() || rule.Dst.String() != filter.Dst.String()):
				continue
			case filterMask&RT_FILTER_TABLE != 0 &&
				filter.Table != unix.RT_TABLE_UNSPEC && rule.Table != filter.Table:
				continue
			case filterMask&RT_FILTER_TOS != 0 && rule.Tos != filter.Tos:
				continue
			case filterMask&RT_FILTER_PRIORITY != 0 && rule.Priority != filter.Priority:
				continue
			case filterMask&RT_FILTER_MARK != 0 && rule.Mark != filter.Mark:
				continue
			case filterMask&RT_FILTER_MASK != 0 && rule.Mask != filter.Mask:
				continue
			}
		}

		res = append(res, *rule)
	}

	return res, nil
}

func (pr *RulePortRange) toRtAttrData() []byte {
	b := [][]byte{make([]byte, 2), make([]byte, 2)}
	native.PutUint16(b[0], pr.Start)
	native.PutUint16(b[1], pr.End)
	return bytes.Join(b, []byte{})
}

func (pr *RuleUIDRange) toRtAttrData() []byte {
	b := [][]byte{make([]byte, 4), make([]byte, 4)}
	native.PutUint32(b[0], pr.Start)
	native.PutUint32(b[1], pr.End)
	return bytes.Join(b, []byte{})
}
