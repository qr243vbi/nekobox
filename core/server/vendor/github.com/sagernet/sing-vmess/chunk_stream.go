package vmess

import (
	"crypto/cipher"
	"io"

	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	N "github.com/sagernet/sing/common/network"
)

type StreamReader struct {
	upstream N.ExtendedReader
	cipher   cipher.Stream
}

func NewStreamReader(upstream io.Reader, key []byte, iv []byte) *StreamReader {
	return &StreamReader{
		upstream: bufio.NewExtendedReader(upstream),
		cipher:   newAesStream(key, iv, cipher.NewCFBDecrypter),
	}
}

func (r *StreamReader) Read(p []byte) (n int, err error) {
	n, err = r.upstream.Read(p)
	if err != nil {
		return
	}
	r.cipher.XORKeyStream(p[:n], p[:n])
	return
}

func (r *StreamReader) ReadBuffer(buffer *buf.Buffer) error {
	err := r.upstream.ReadBuffer(buffer)
	if err != nil {
		return err
	}
	r.cipher.XORKeyStream(buffer.Bytes(), buffer.Bytes())
	return nil
}

func (r *StreamReader) Upstream() any {
	return r.upstream
}

type StreamWriter struct {
	upstream N.ExtendedWriter
	cipher   cipher.Stream
}

func NewStreamWriter(upstream io.Writer, key []byte, iv []byte) *StreamWriter {
	return &StreamWriter{
		upstream: bufio.NewExtendedWriter(upstream),
		cipher:   newAesStream(key, iv, cipher.NewCFBEncrypter),
	}
}

func (w *StreamWriter) Write(p []byte) (n int, err error) {
	w.cipher.XORKeyStream(p, p)
	return w.upstream.Write(p)
}

func (w *StreamWriter) WriteBuffer(buffer *buf.Buffer) error {
	w.cipher.XORKeyStream(buffer.Bytes(), buffer.Bytes())
	return w.upstream.WriteBuffer(buffer)
}

func (w *StreamWriter) Upstream() any {
	return w.upstream
}
