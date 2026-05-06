package option

import (
	"net/netip"

	"github.com/sagernet/sing/common/json/badoption"
)

type AwgEndpointOptions struct {
	UseIntegratedTun bool                             `json:"useIntegratedTun"`
	PrivateKey       string                           `json:"private_key"`
	Address          badoption.Listable[netip.Prefix] `json:"address"`
	MTU              uint32                           `json:"mtu,omitempty"`
	ListenPort       uint16                           `json:"listen_port,omitempty"`
	Jc               int                              `json:"jc,omitempty"`
	Jmin             int                              `json:"jmin,omitempty"`
	Jmax             int                              `json:"jmax,omitempty"`
	S1               int                              `json:"s1,omitempty"`
	S2               int                              `json:"s2,omitempty"`
	S3               int                              `json:"s3,omitempty"`
	S4               int                              `json:"s4,omitempty"`
	H1               string                           `json:"h1,omitempty"`
	H2               string                           `json:"h2,omitempty"`
	H3               string                           `json:"h3,omitempty"`
	H4               string                           `json:"h4,omitempty"`
	I1               string                           `json:"i1,omitempty"`
	I2               string                           `json:"i2,omitempty"`
	I3               string                           `json:"i3,omitempty"`
	I4               string                           `json:"i4,omitempty"`
	I5               string                           `json:"i5,omitempty"`
	Peers            []AwgPeerOptions                 `json:"peers,omitempty"`
	DialerOptions
}

type AwgPeerOptions struct {
	Address                     string                           `json:"address,omitempty"`
	Port                        uint16                           `json:"port,omitempty"`
	PublicKey                   string                           `json:"public_key,omitempty"`
	PresharedKey                string                           `json:"preshared_key,omitempty"`
	AllowedIPs                  badoption.Listable[netip.Prefix] `json:"allowed_ips,omitempty"`
	PersistentKeepaliveInterval uint16                           `json:"persistent_keepalive_interval,omitempty"`
}
