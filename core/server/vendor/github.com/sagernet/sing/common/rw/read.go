package rw

import (
	"io"

	"github.com/sagernet/sing/common"
)

func SkipN(reader io.Reader, size int) error {
	return common.Error(io.CopyN(Discard, reader, int64(size)))
}

// Deprecated: wtf is this?
func Skip(reader io.Reader) error {
	return SkipN(reader, 1)
}

// Deprecated: wtf is this?
func ReadByte(reader io.Reader) (byte, error) {
	if br, isBr := reader.(io.ByteReader); isBr {
		return br.ReadByte()
	}
	var b [1]byte
	if err := common.Error(io.ReadFull(reader, b[:])); err != nil {
		return 0, err
	}
	return b[0], nil
}

// Deprecated: wtf is this?
func ReadBytes(reader io.Reader, size int) ([]byte, error) {
	b := make([]byte, size)
	if err := common.Error(io.ReadFull(reader, b)); err != nil {
		return nil, err
	}
	return b, nil
}

// Deprecated: wtf is this?
func ReadString(reader io.Reader, size int) (string, error) {
	b, err := ReadBytes(reader, size)
	if err != nil {
		return "", err
	}
	return string(b), nil
}
