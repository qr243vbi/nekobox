package netlink

import (
	"fmt"
	"net/netip"
)

// Rule represents a netlink rule.
type Rule struct {
	Priority          int
	Family            int
	Table             int
	Type              uint8
	Mark              uint32
	MarkSet           bool
	Mask              int
	Tos               uint
	TunID             uint
	Goto              int
	Src               netip.Prefix
	Dst               netip.Prefix
	Flow              int
	IifName           string
	OifName           string
	SuppressIfgroup   int
	SuppressPrefixlen int
	Invert            bool
	Dport             *RulePortRange
	Sport             *RulePortRange
	IPProto           int
	UIDRange          *RuleUIDRange
}

func (r Rule) String() string {
	from := "all"
	if r.Src.IsValid() && r.Src.String() != "<nil>" {
		from = r.Src.String()
	}

	to := "all"
	if r.Dst.IsValid() && r.Dst.String() != "<nil>" {
		to = r.Dst.String()
	}

	return fmt.Sprintf("ip rule %d: from %s to %s table %d",
		r.Priority, from, to, r.Table)
}

// NewRule return empty rules.
func NewRule() *Rule {
	return &Rule{
		Table:             -1,
		SuppressIfgroup:   -1,
		SuppressPrefixlen: -1,
		Priority:          -1,
		Mask:              -1,
		Goto:              -1,
		Flow:              -1,
	}
}

// NewRulePortRange creates rule sport/dport range.
func NewRulePortRange(start, end uint16) *RulePortRange {
	return &RulePortRange{Start: start, End: end}
}

// RulePortRange represents rule sport/dport range.
type RulePortRange struct {
	Start uint16
	End   uint16
}

// NewRuleUIDRange creates rule uid range.
func NewRuleUIDRange(start, end uint32) *RuleUIDRange {
	return &RuleUIDRange{Start: start, End: end}
}

// RuleUIDRange represents rule uid range.
type RuleUIDRange struct {
	Start uint32
	End   uint32
}
