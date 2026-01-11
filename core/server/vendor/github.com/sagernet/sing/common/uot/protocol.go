package uot

import (
	"encoding/binary"
	"io"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
)

const (
	Version            = 2
	LegacyVersion      = 1
	MagicAddress       = "sp.v2.udp-over-tcp.arpa"
	LegacyMagicAddress = "sp.udp-over-tcp.arpa"
)

var AddrParser = M.NewSerializer(
	M.AddressFamilyByte(0x00, M.AddressFamilyIPv4),
	M.AddressFamilyByte(0x01, M.AddressFamilyIPv6),
	M.AddressFamilyByte(0x02, M.AddressFamilyFqdn),
)

func RequestDestination(version uint8) M.Socksaddr {
	switch version {
	case 0, Version:
		return M.Socksaddr{Fqdn: MagicAddress}
	default:
		fallthrough
	case LegacyVersion:
		return M.Socksaddr{Fqdn: LegacyMagicAddress}
	}
}

type Request struct {
	IsConnect   bool
	Destination M.Socksaddr
}

func ReadRequest(reader io.Reader) (*Request, error) {
	var request Request
	err := binary.Read(reader, binary.BigEndian, &request.IsConnect)
	if err != nil {
		return nil, err
	}
	request.Destination, err = M.SocksaddrSerializer.ReadAddrPort(reader)
	if err != nil {
		return nil, err
	}
	return &request, nil
}

func EncodeRequest(request Request) (*buf.Buffer, error) {
	var bufferLen int
	bufferLen += 1 // isConnect
	bufferLen += M.SocksaddrSerializer.AddrPortLen(request.Destination)
	buffer := buf.NewSize(bufferLen)
	common.Must(binary.Write(buffer, binary.BigEndian, request.IsConnect))
	err := M.SocksaddrSerializer.WriteAddrPort(buffer, request.Destination)
	if err != nil {
		buffer.Release()
		return nil, err
	}
	return buffer, nil
}

func WriteRequest(writer io.Writer, request Request) error {
	buffer, err := EncodeRequest(request)
	if err != nil {
		return err
	}
	defer buffer.Release()
	return common.Error(writer.Write(buffer.Bytes()))
}
