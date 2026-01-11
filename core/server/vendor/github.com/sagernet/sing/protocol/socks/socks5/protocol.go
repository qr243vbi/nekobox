package socks5

import (
	"errors"
	"io"
	"net/netip"
	"syscall"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	"github.com/sagernet/sing/common/varbin"
)

const (
	Version byte = 5

	AuthTypeNotRequired       byte = 0x00
	AuthTypeGSSAPI            byte = 0x01
	AuthTypeUsernamePassword  byte = 0x02
	AuthTypeNoAcceptedMethods byte = 0xFF

	UsernamePasswordStatusSuccess byte = 0x00
	UsernamePasswordStatusFailure byte = 0x01

	CommandConnect      byte = 0x01
	CommandBind         byte = 0x02
	CommandUDPAssociate byte = 0x03

	ReplyCodeSuccess                byte = 0
	ReplyCodeFailure                byte = 1
	ReplyCodeNotAllowed             byte = 2
	ReplyCodeNetworkUnreachable     byte = 3
	ReplyCodeHostUnreachable        byte = 4
	ReplyCodeConnectionRefused      byte = 5
	ReplyCodeTTLExpired             byte = 6
	ReplyCodeUnsupported            byte = 7
	ReplyCodeAddressTypeUnsupported byte = 8
)

func ReplyCodeForError(err error) byte {
	if errors.Is(err, syscall.ENETUNREACH) {
		return ReplyCodeNetworkUnreachable
	} else if errors.Is(err, syscall.EHOSTUNREACH) {
		return ReplyCodeHostUnreachable
	} else if errors.Is(err, syscall.ECONNREFUSED) {
		return ReplyCodeConnectionRefused
	} else if errors.Is(err, syscall.EPERM) {
		return ReplyCodeNotAllowed
	} else {
		return ReplyCodeFailure
	}
}

// +----+----------+----------+
// |VER | NMETHODS | METHODS  |
// +----+----------+----------+
// | 1  |    1     | 1 to 255 |
// +----+----------+----------+

type AuthRequest struct {
	Methods []byte
}

func WriteAuthRequest(writer io.Writer, request AuthRequest) error {
	buffer := buf.NewSize(len(request.Methods) + 2)
	defer buffer.Release()
	common.Must(
		buffer.WriteByte(Version),
		buffer.WriteByte(byte(len(request.Methods))),
		common.Error(buffer.Write(request.Methods)),
	)
	return common.Error(writer.Write(buffer.Bytes()))
}

func ReadAuthRequest(reader varbin.Reader) (request AuthRequest, err error) {
	version, err := reader.ReadByte()
	if err != nil {
		return
	}
	if version != Version {
		err = E.New("expected socks version 5, got ", version)
		return
	}
	return ReadAuthRequest0(reader)
}

func ReadAuthRequest0(reader varbin.Reader) (request AuthRequest, err error) {
	methodLen, err := reader.ReadByte()
	if err != nil {
		return
	}
	request.Methods = make([]byte, methodLen)
	_, err = io.ReadFull(reader, request.Methods)
	return
}

// +----+--------+
// |VER | METHOD |
// +----+--------+
// | 1  |   1    |
// +----+--------+

type AuthResponse struct {
	Method byte
}

func WriteAuthResponse(writer io.Writer, response AuthResponse) error {
	return common.Error(writer.Write([]byte{Version, response.Method}))
}

func ReadAuthResponse(reader varbin.Reader) (response AuthResponse, err error) {
	version, err := reader.ReadByte()
	if err != nil {
		return
	}
	if version != Version {
		err = E.New("expected socks version 5, got ", version)
		return
	}
	response.Method, err = reader.ReadByte()
	return
}

// +----+------+----------+------+----------+
// |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
// +----+------+----------+------+----------+
// | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
// +----+------+----------+------+----------+

type UsernamePasswordAuthRequest struct {
	Username string
	Password string
}

func WriteUsernamePasswordAuthRequest(writer io.Writer, request UsernamePasswordAuthRequest) error {
	buffer := buf.NewSize(3 + len(request.Username) + len(request.Password))
	defer buffer.Release()
	common.Must(
		buffer.WriteByte(1),
		M.WriteSocksString(buffer, request.Username),
		M.WriteSocksString(buffer, request.Password),
	)
	return common.Error(writer.Write(buffer.Bytes()))
}

func ReadUsernamePasswordAuthRequest(reader varbin.Reader) (request UsernamePasswordAuthRequest, err error) {
	version, err := reader.ReadByte()
	if err != nil {
		return
	}
	if version != 1 {
		err = E.New("excepted password request version 1, got ", version)
		return
	}
	request.Username, err = M.ReadSockString(reader)
	if err != nil {
		return
	}
	request.Password, err = M.ReadSockString(reader)
	if err != nil {
		return
	}
	return
}

// +----+--------+
// |VER | STATUS |
// +----+--------+
// | 1  |   1    |
// +----+--------+

type UsernamePasswordAuthResponse struct {
	Status byte
}

func WriteUsernamePasswordAuthResponse(writer io.Writer, response UsernamePasswordAuthResponse) error {
	return common.Error(writer.Write([]byte{1, response.Status}))
}

func ReadUsernamePasswordAuthResponse(reader varbin.Reader) (response UsernamePasswordAuthResponse, err error) {
	version, err := reader.ReadByte()
	if err != nil {
		return
	}
	if version != 1 {
		err = E.New("excepted password request version 1, got ", version)
		return
	}
	response.Status, err = reader.ReadByte()
	return
}

// +----+-----+-------+------+----------+----------+
// |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
// +----+-----+-------+------+----------+----------+
// | 1  |  1  | X'00' |  1   | Variable |    2     |
// +----+-----+-------+------+----------+----------+

type Request struct {
	Command     byte
	Destination M.Socksaddr
}

func WriteRequest(writer io.Writer, request Request) error {
	buffer := buf.NewSize(3 + M.SocksaddrSerializer.AddrPortLen(request.Destination))
	defer buffer.Release()
	common.Must(
		buffer.WriteByte(Version),
		buffer.WriteByte(request.Command),
		buffer.WriteZero(),
	)
	err := M.SocksaddrSerializer.WriteAddrPort(buffer, request.Destination)
	if err != nil {
		return err
	}
	return common.Error(writer.Write(buffer.Bytes()))
}

func ReadRequest(reader varbin.Reader) (request Request, err error) {
	version, err := reader.ReadByte()
	if err != nil {
		return
	}
	if version != Version {
		err = E.New("expected socks version 5, got ", version)
		return
	}
	request.Command, err = reader.ReadByte()
	if err != nil {
		return
	}
	_, err = reader.ReadByte()
	if err != nil {
		return
	}
	request.Destination, err = M.SocksaddrSerializer.ReadAddrPort(reader)
	return
}

// +----+-----+-------+------+----------+----------+
// |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
// +----+-----+-------+------+----------+----------+
// | 1  |  1  | X'00' |  1   | Variable |    2     |
// +----+-----+-------+------+----------+----------+

type Response struct {
	ReplyCode byte
	Bind      M.Socksaddr
}

func WriteResponse(writer io.Writer, response Response) error {
	var bind M.Socksaddr
	if response.Bind.IsValid() {
		bind = response.Bind
	} else {
		bind.Addr = netip.IPv4Unspecified()
	}

	buffer := buf.NewSize(3 + M.SocksaddrSerializer.AddrPortLen(bind))
	defer buffer.Release()
	common.Must(
		buffer.WriteByte(Version),
		buffer.WriteByte(response.ReplyCode),
		buffer.WriteZero(),
	)
	err := M.SocksaddrSerializer.WriteAddrPort(buffer, bind)
	if err != nil {
		return err
	}
	return common.Error(writer.Write(buffer.Bytes()))
}

func ReadResponse(reader varbin.Reader) (response Response, err error) {
	version, err := reader.ReadByte()
	if err != nil {
		return
	}
	if version != Version {
		err = E.New("expected socks version 5, got ", version)
		return
	}
	response.ReplyCode, err = reader.ReadByte()
	if err != nil {
		return
	}
	_, err = reader.ReadByte()
	if err != nil {
		return
	}
	response.Bind, err = M.SocksaddrSerializer.ReadAddrPort(reader)
	return
}
