package metadata

var SocksaddrSerializer = NewSerializer(
	AddressFamilyByte(0x01, AddressFamilyIPv4),
	AddressFamilyByte(0x04, AddressFamilyIPv6),
	AddressFamilyByte(0x03, AddressFamilyFqdn),
)
