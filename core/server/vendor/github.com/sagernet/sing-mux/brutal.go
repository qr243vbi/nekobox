package mux

import (
	"encoding/binary"
	"io"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/varbin"
)

const (
	BrutalExchangeDomain = "_BrutalBwExchange"
	BrutalMinSpeedBPS    = 65536
)

func WriteBrutalRequest(writer io.Writer, receiveBPS uint64) error {
	return binary.Write(writer, binary.BigEndian, receiveBPS)
}

func ReadBrutalRequest(reader io.Reader) (uint64, error) {
	var receiveBPS uint64
	err := binary.Read(reader, binary.BigEndian, &receiveBPS)
	return receiveBPS, err
}

func WriteBrutalResponse(writer io.Writer, receiveBPS uint64, ok bool, message string) error {
	buffer := buf.New()
	defer buffer.Release()
	common.Must(binary.Write(buffer, binary.BigEndian, ok))
	if ok {
		common.Must(binary.Write(buffer, binary.BigEndian, receiveBPS))
	} else {
		err := varbin.Write(buffer, binary.BigEndian, message)
		if err != nil {
			return err
		}
	}
	return common.Error(writer.Write(buffer.Bytes()))
}

func ReadBrutalResponse(reader io.Reader) (uint64, error) {
	var ok bool
	err := binary.Read(reader, binary.BigEndian, &ok)
	if err != nil {
		return 0, err
	}
	if ok {
		var receiveBPS uint64
		err = binary.Read(reader, binary.BigEndian, &receiveBPS)
		return receiveBPS, err
	} else {
		var message string
		message, err = varbin.ReadValue[string](reader, binary.BigEndian)
		if err != nil {
			return 0, err
		}
		return 0, E.New("remote error: ", message)
	}
}
