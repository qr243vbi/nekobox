package rw

import (
	"io"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/binary"
	"github.com/sagernet/sing/common/varbin"
)

// Deprecated: create a *bufio.Reader instead.
type stubByteReader struct {
	io.Reader
}

func (r stubByteReader) ReadByte() (byte, error) {
	return ReadByte(r.Reader)
}

// Deprecated: create a *bufio.Reader instead.
func ToByteReader(reader io.Reader) io.ByteReader {
	if byteReader, ok := reader.(io.ByteReader); ok {
		return byteReader
	}
	return &stubByteReader{reader}
}

// Deprecated: Use binary.ReadUvarint instead.
func ReadUVariant(reader io.Reader) (uint64, error) {
	return binary.ReadUvarint(ToByteReader(reader))
}

// Deprecated: Use varbin.UvarintLen instead.
func UVariantLen(x uint64) int {
	return varbin.UvarintLen(x)
}

// Deprecated: Use varbin.WriteUvarint instead.
func WriteUVariant(writer io.Writer, value uint64) error {
	var b [8]byte
	return common.Error(writer.Write(b[:binary.PutUvarint(b[:], value)]))
}

// Deprecated: Use varbin.Write instead.
func WriteVString(writer io.Writer, value string) error {
	err := WriteUVariant(writer, uint64(len(value)))
	if err != nil {
		return err
	}
	return WriteString(writer, value)
}

// Deprecated: Use varbin.ReadValue instead.
func ReadVString(reader io.Reader) (string, error) {
	length, err := binary.ReadUvarint(ToByteReader(reader))
	if err != nil {
		return "", err
	}
	value, err := ReadBytes(reader, int(length))
	if err != nil {
		return "", err
	}
	return string(value), nil
}
