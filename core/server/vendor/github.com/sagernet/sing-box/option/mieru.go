package option

import "github.com/sagernet/sing/common/json/badoption"

type MieruOutboundOptions struct {
	DialerOptions
	ServerOptions
	ServerPortRanges badoption.Listable[string] `json:"server_ports,omitempty"`
	Transport        string                     `json:"transport,omitempty"`
	UserName         string                     `json:"username,omitempty"`
	Password         string                     `json:"password,omitempty"`
	Multiplexing     string                     `json:"multiplexing,omitempty"`
	TrafficPattern   string                     `json:"traffic_pattern,omitempty"`
}

type MieruInboundOptions struct {
	ListenOptions
	Users          []MieruUser `json:"users,omitempty"`
	Transport      string      `json:"transport,omitempty"`
	TrafficPattern string      `json:"traffic_pattern,omitempty"`
}

type MieruUser struct {
	Name     string `json:"name,omitempty"`
	Password string `json:"password,omitempty"`
}
