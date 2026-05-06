package metadata

type Family = byte

const (
	AddressFamilyIPv4  Family = 0x01
	AddressFamilyIPv6  Family = 0x04
	AddressFamilyFqdn  Family = 0x03
	AddressFamilyEmpty Family = 0xff
)
