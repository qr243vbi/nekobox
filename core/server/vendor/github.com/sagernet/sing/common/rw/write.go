package rw

import (
	"io"

	"github.com/sagernet/sing/common"
)

// Deprecated: wtf is this?
var ZeroBytes = make([]byte, 1024)

// Deprecated: wtf is this?
func WriteZeroN(writer io.Writer, size int) error {
	var index int
	for index < size {
		next := index + 1024
		if next > size {
			_, err := writer.Write(ZeroBytes[:size-index])
			return err
		} else {
			_, err := writer.Write(ZeroBytes)
			if err != nil {
				return err
			}
			index = next
		}
	}
	return nil
}

// Deprecated: wtf is this?
func WriteByte(writer io.Writer, b byte) error {
	return common.Error(writer.Write([]byte{b}))
}

// Deprecated: wtf is this?
func WriteBytes(writer io.Writer, b []byte) error {
	return common.Error(writer.Write(b))
}

// Deprecated: wtf is this?
func WriteZero(writer io.Writer) error {
	return WriteByte(writer, 0)
}

// Deprecated: wtf is this?
func WriteString(writer io.Writer, str string) error {
	return WriteBytes(writer, []byte(str))
}
