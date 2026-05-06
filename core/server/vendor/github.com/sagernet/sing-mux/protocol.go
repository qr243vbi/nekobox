package mux

import (
	"encoding/binary"
	"io"
	"math/rand"
	"time"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/rw"
	"github.com/sagernet/sing/common/varbin"
)

const (
	ProtocolSmux = iota
	ProtocolYAMux
	ProtocolH2Mux
)

const (
	Version0 = iota
	Version1
)

const (
	TCPTimeout = 5 * time.Second
)

var Destination = M.Socksaddr{
	Fqdn: "sp.mux.sing-box.arpa",
	Port: 444,
}

type Request struct {
	Version  byte
	Protocol byte
	Padding  bool
}

func ReadRequest(reader io.Reader) (*Request, error) {
	var (
		version  byte
		protocol byte
	)
	err := binary.Read(reader, binary.BigEndian, &version)
	if err != nil {
		return nil, err
	}
	if version < Version0 || version > Version1 {
		return nil, E.New("unsupported version: ", version)
	}
	err = binary.Read(reader, binary.BigEndian, &protocol)
	if err != nil {
		return nil, err
	}
	var paddingEnabled bool
	if version == Version1 {
		err = binary.Read(reader, binary.BigEndian, &paddingEnabled)
		if err != nil {
			return nil, err
		}
		if paddingEnabled {
			var paddingLen uint16
			err = binary.Read(reader, binary.BigEndian, &paddingLen)
			if err != nil {
				return nil, err
			}
			err = rw.SkipN(reader, int(paddingLen))
			if err != nil {
				return nil, err
			}
		}
	}
	return &Request{Version: version, Protocol: protocol, Padding: paddingEnabled}, nil
}

func EncodeRequest(request Request, payload []byte) *buf.Buffer {
	var requestLen int
	requestLen += 2
	var paddingLen uint16
	if request.Version == Version1 {
		requestLen += 1
		if request.Padding {
			requestLen += 2
			paddingLen = uint16(256 + rand.Intn(512))
			requestLen += int(paddingLen)
		}
	}
	buffer := buf.NewSize(requestLen + len(payload))
	common.Must(
		buffer.WriteByte(request.Version),
		buffer.WriteByte(request.Protocol),
	)
	if request.Version == Version1 {
		common.Must(binary.Write(buffer, binary.BigEndian, request.Padding))
		if request.Padding {
			common.Must(binary.Write(buffer, binary.BigEndian, paddingLen))
			buffer.Extend(int(paddingLen))
		}
	}
	common.Must1(buffer.Write(payload))
	return buffer
}

const (
	flagUDP       = 1
	flagAddr      = 2
	statusSuccess = 0
	statusError   = 1
)

type StreamRequest struct {
	Network     string
	Destination M.Socksaddr
	PacketAddr  bool
}

func ReadStreamRequest(reader io.Reader) (*StreamRequest, error) {
	var flags uint16
	err := binary.Read(reader, binary.BigEndian, &flags)
	if err != nil {
		return nil, err
	}
	destination, err := M.SocksaddrSerializer.ReadAddrPort(reader)
	if err != nil {
		return nil, err
	}
	var network string
	var udpAddr bool
	if flags&flagUDP == 0 {
		network = N.NetworkTCP
	} else {
		network = N.NetworkUDP
		udpAddr = flags&flagAddr != 0
	}
	return &StreamRequest{network, destination, udpAddr}, nil
}

func streamRequestLen(request StreamRequest) int {
	var rLen int
	rLen += 1 // version
	rLen += 2 // flags
	rLen += M.SocksaddrSerializer.AddrPortLen(request.Destination)
	return rLen
}

func EncodeStreamRequest(request StreamRequest, buffer *buf.Buffer) error {
	destination := request.Destination
	var flags uint16
	if request.Network == N.NetworkUDP {
		flags |= flagUDP
	}
	if request.PacketAddr {
		flags |= flagAddr
		if !destination.IsValid() {
			destination = Destination
		}
	}
	common.Must(binary.Write(buffer, binary.BigEndian, flags))
	return M.SocksaddrSerializer.WriteAddrPort(buffer, destination)
}

type StreamResponse struct {
	Status  uint8
	Message string
}

func ReadStreamResponse(reader io.Reader) (*StreamResponse, error) {
	var response StreamResponse
	err := binary.Read(reader, binary.BigEndian, &response.Status)
	if err != nil {
		return nil, err
	}
	if response.Status == statusError {
		response.Message, err = varbin.ReadValue[string](reader, binary.BigEndian)
		if err != nil {
			return nil, err
		}
	}
	return &response, nil
}
