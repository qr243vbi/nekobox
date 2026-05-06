package option

type VLESSInboundOptions struct {
	ListenOptions
	Users []VLESSUser `json:"users,omitempty"`
	InboundTLSOptionsContainer
	Multiplex   *InboundMultiplexOptions `json:"multiplex,omitempty"`
	Transport   *V2RayTransportOptions   `json:"transport,omitempty"`
	Decryption  string                   `json:"decryption,omitempty"`
	XorMode     uint32                   `json:"xor_mode,omitempty"`
	SecondsFrom int64                    `json:"seconds_from,omitempty"`
	SecondsTo   int64                    `json:"seconds_to,omitempty"`
	Padding     string                   `json:"padding,omitempty"`
}

type VLESSUser struct {
	Name string `json:"name"`
	UUID string `json:"uuid"`
	Flow string `json:"flow,omitempty"`
}

type VLESSOutboundOptions struct {
	DialerOptions
	ServerOptions
	UUID    string      `json:"uuid"`
	Flow    string      `json:"flow,omitempty"`
	Network NetworkList `json:"network,omitempty"`
	OutboundTLSOptionsContainer
	Multiplex      *OutboundMultiplexOptions `json:"multiplex,omitempty"`
	Transport      *V2RayTransportOptions    `json:"transport,omitempty"`
	PacketEncoding *string                   `json:"packet_encoding,omitempty"`
	Encryption     string                    `json:"encryption,omitempty"`
}
