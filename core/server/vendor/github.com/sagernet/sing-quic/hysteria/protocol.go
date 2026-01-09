package hysteria

import (
	"encoding/binary"
	"io"
	"time"

	"github.com/sagernet/quic-go"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
)

const (
	MbpsToBps                  = 125000
	MinSpeedBPS                = 16384
	DefaultALPN                = "hysteria"
	DefaultStreamReceiveWindow = 8388608                            // 8MB
	DefaultConnReceiveWindow   = DefaultStreamReceiveWindow * 5 / 2 // 20MB
	DefaultMaxIdleTimeout      = 30 * time.Second
	DefaultKeepAlivePeriod     = 10 * time.Second
)

const (
	ProtocolVersion        = 3
	ProtocolTimeout        = 10 * time.Second
	ErrorCodeGeneric       = 0
	ErrorCodeProtocolError = 1
	ErrorCodeAuthError     = 2
)

type ClientHello struct {
	SendBPS uint64
	RecvBPS uint64
	Auth    string
}

func WriteClientHello(stream io.Writer, hello ClientHello) error {
	var requestLen int
	requestLen += 1 // version
	requestLen += 8 // sendBPS
	requestLen += 8 // recvBPS
	requestLen += 2 // auth len
	requestLen += len(hello.Auth)
	request := buf.NewSize(requestLen)
	defer request.Release()
	common.Must(
		request.WriteByte(ProtocolVersion),
		binary.Write(request, binary.BigEndian, hello.SendBPS),
		binary.Write(request, binary.BigEndian, hello.RecvBPS),
		binary.Write(request, binary.BigEndian, uint16(len(hello.Auth))),
		common.Error(request.WriteString(hello.Auth)),
	)
	return common.Error(stream.Write(request.Bytes()))
}

func ReadClientHello(reader io.Reader) (*ClientHello, error) {
	var version uint8
	err := binary.Read(reader, binary.BigEndian, &version)
	if err != nil {
		return nil, err
	}
	if version != ProtocolVersion {
		return nil, E.New("unsupported client version: ", version)
	}
	var clientHello ClientHello
	err = binary.Read(reader, binary.BigEndian, &clientHello.SendBPS)
	if err != nil {
		return nil, err
	}
	err = binary.Read(reader, binary.BigEndian, &clientHello.RecvBPS)
	if err != nil {
		return nil, err
	}
	if clientHello.SendBPS == 0 || clientHello.RecvBPS == 0 {
		return nil, E.New("invalid rate from client")
	}
	var authLen uint16
	err = binary.Read(reader, binary.BigEndian, &authLen)
	if err != nil {
		return nil, err
	}
	authBytes := make([]byte, authLen)
	_, err = io.ReadFull(reader, authBytes)
	if err != nil {
		return nil, err
	}
	clientHello.Auth = string(authBytes)
	return &clientHello, nil
}

type ServerHello struct {
	OK      bool
	SendBPS uint64
	RecvBPS uint64
	Message string
}

func ReadServerHello(stream io.Reader) (*ServerHello, error) {
	var responseLen int
	responseLen += 1 // ok
	responseLen += 8 // sendBPS
	responseLen += 8 // recvBPS
	responseLen += 2 // message len
	response := buf.NewSize(responseLen)
	defer response.Release()
	_, err := response.ReadFullFrom(stream, responseLen)
	if err != nil {
		return nil, err
	}
	var serverHello ServerHello
	serverHello.OK = response.Byte(0) == 1
	serverHello.SendBPS = binary.BigEndian.Uint64(response.Range(1, 9))
	serverHello.RecvBPS = binary.BigEndian.Uint64(response.Range(9, 17))
	messageLen := binary.BigEndian.Uint16(response.Range(17, 19))
	if messageLen == 0 {
		return &serverHello, nil
	}
	message := make([]byte, messageLen)
	_, err = io.ReadFull(stream, message)
	if err != nil {
		return nil, err
	}
	serverHello.Message = string(message)
	return &serverHello, nil
}

func WriteServerHello(stream io.Writer, hello ServerHello) error {
	var responseLen int
	responseLen += 1 // ok
	responseLen += 8 // sendBPS
	responseLen += 8 // recvBPS
	responseLen += 2 // message len
	responseLen += len(hello.Message)
	response := buf.NewSize(responseLen)
	defer response.Release()
	if hello.OK {
		common.Must(response.WriteByte(1))
	} else {
		common.Must(response.WriteByte(0))
	}
	common.Must(
		binary.Write(response, binary.BigEndian, hello.SendBPS),
		binary.Write(response, binary.BigEndian, hello.RecvBPS),
		binary.Write(response, binary.BigEndian, uint16(len(hello.Message))),
		common.Error(response.WriteString(hello.Message)),
	)
	return common.Error(stream.Write(response.Bytes()))
}

type ClientRequest struct {
	UDP  bool
	Host string
	Port uint16
}

func ReadClientRequest(stream io.Reader) (*ClientRequest, error) {
	var clientRequest ClientRequest
	err := binary.Read(stream, binary.BigEndian, &clientRequest.UDP)
	if err != nil {
		return nil, err
	}
	var hostLen uint16
	err = binary.Read(stream, binary.BigEndian, &hostLen)
	if err != nil {
		return nil, err
	}
	host := make([]byte, hostLen)
	_, err = io.ReadFull(stream, host)
	if err != nil {
		return nil, err
	}
	clientRequest.Host = string(host)
	err = binary.Read(stream, binary.BigEndian, &clientRequest.Port)
	if err != nil {
		return nil, err
	}
	return &clientRequest, nil
}

func WriteClientRequest(request ClientRequest, payload []byte) *buf.Buffer {
	var requestLen int
	requestLen += 1 // udp
	requestLen += 2 // host len
	requestLen += len(request.Host)
	requestLen += 2 // port
	buffer := buf.NewSize(requestLen + len(payload))
	if request.UDP {
		common.Must(buffer.WriteByte(1))
	} else {
		common.Must(buffer.WriteByte(0))
	}
	common.Must(
		binary.Write(buffer, binary.BigEndian, uint16(len(request.Host))),
		common.Error(buffer.WriteString(request.Host)),
		binary.Write(buffer, binary.BigEndian, request.Port),
		common.Error(buffer.Write(payload)),
	)
	return buffer
}

type ServerResponse struct {
	OK           bool
	UDPSessionID uint32
	Message      string
}

func ReadServerResponse(stream io.Reader) (*ServerResponse, error) {
	var responseLen int
	responseLen += 1 // ok
	responseLen += 4 // udp session id
	responseLen += 2 // message len
	response := buf.NewSize(responseLen)
	defer response.Release()
	_, err := response.ReadFullFrom(stream, responseLen)
	if err != nil {
		return nil, err
	}
	var serverResponse ServerResponse
	serverResponse.OK = response.Byte(0) == 1
	serverResponse.UDPSessionID = binary.BigEndian.Uint32(response.Range(1, 5))
	messageLen := binary.BigEndian.Uint16(response.Range(5, 7))
	if messageLen == 0 {
		return &serverResponse, nil
	}
	message := make([]byte, messageLen)
	_, err = io.ReadFull(stream, message)
	if err != nil {
		return nil, err
	}
	serverResponse.Message = string(message)
	return &serverResponse, nil
}

func WriteServerResponse(stream quic.Stream, response ServerResponse) error {
	var responseLen int
	responseLen += 1 // ok
	responseLen += 4 // udp session id
	responseLen += 2 // message len
	responseLen += len(response.Message)
	buffer := buf.NewSize(responseLen)
	defer buffer.Release()
	if response.OK {
		common.Must(buffer.WriteByte(1))
	} else {
		common.Must(buffer.WriteByte(0))
	}
	common.Must(
		binary.Write(buffer, binary.BigEndian, response.UDPSessionID),
		binary.Write(buffer, binary.BigEndian, uint16(len(response.Message))),
		common.Error(buffer.WriteString(response.Message)),
	)
	return common.Error(stream.Write(buffer.Bytes()))
}
