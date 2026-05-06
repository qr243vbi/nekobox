package badoption

import (
	"net/netip"

	"github.com/sagernet/sing/common/json"
)

type Addr netip.Addr

func (a *Addr) Build(defaultAddr netip.Addr) netip.Addr {
	if a == nil {
		return defaultAddr
	}
	return netip.Addr(*a)
}

func (a *Addr) MarshalJSON() ([]byte, error) {
	return json.Marshal(netip.Addr(*a).String())
}

func (a *Addr) UnmarshalJSON(content []byte) error {
	var value string
	err := json.Unmarshal(content, &value)
	if err != nil {
		return err
	}
	addr, err := netip.ParseAddr(value)
	if err != nil {
		return err
	}
	*a = Addr(addr)
	return nil
}

type Prefix netip.Prefix

func (p *Prefix) Build(defaultPrefix netip.Prefix) netip.Prefix {
	if p == nil {
		return defaultPrefix
	}
	return netip.Prefix(*p)
}

func (p *Prefix) MarshalJSON() ([]byte, error) {
	return json.Marshal(netip.Prefix(*p).String())
}

func (p *Prefix) UnmarshalJSON(content []byte) error {
	var value string
	err := json.Unmarshal(content, &value)
	if err != nil {
		return err
	}
	prefix, err := netip.ParsePrefix(value)
	if err != nil {
		return err
	}
	*p = Prefix(prefix)
	return nil
}

type Prefixable netip.Prefix

func (p *Prefixable) Build(defaultPrefix netip.Prefix) netip.Prefix {
	if p == nil {
		return defaultPrefix
	}
	return netip.Prefix(*p)
}

func (p *Prefixable) MarshalJSON() ([]byte, error) {
	prefix := netip.Prefix(*p)
	if prefix.Bits() == prefix.Addr().BitLen() {
		return json.Marshal(prefix.Addr().String())
	} else {
		return json.Marshal(prefix.String())
	}
}

func (p *Prefixable) UnmarshalJSON(content []byte) error {
	var value string
	err := json.Unmarshal(content, &value)
	if err != nil {
		return err
	}
	prefix, prefixErr := netip.ParsePrefix(value)
	if prefixErr == nil {
		*p = Prefixable(prefix)
		return nil
	}
	addr, addrErr := netip.ParseAddr(value)
	if addrErr == nil {
		*p = Prefixable(netip.PrefixFrom(addr, addr.BitLen()))
		return nil
	}
	return prefixErr
}
