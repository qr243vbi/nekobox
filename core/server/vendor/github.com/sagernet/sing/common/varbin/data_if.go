package varbin

import (
	"io"
)

type Reader interface {
	io.Reader
	io.ByteReader
}

type Writer interface {
	io.Writer
	io.ByteWriter
}

var _ Reader = stubReader{}

func StubReader(reader io.Reader) Reader {
	if r, ok := reader.(Reader); ok {
		return r
	}
	return stubReader{reader}
}

type stubReader struct {
	io.Reader
}

func (r stubReader) ReadByte() (byte, error) {
	var b [1]byte
	_, err := r.Read(b[:])
	return b[0], err
}
