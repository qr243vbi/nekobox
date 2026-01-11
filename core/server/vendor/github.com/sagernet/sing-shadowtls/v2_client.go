package shadowtls

import (
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
)

type clientConn struct {
	*shadowConn
	hashConn *hashReadConn
}

func newClientConn(hashConn *hashReadConn) *clientConn {
	return &clientConn{newConn(hashConn.Conn), hashConn}
}

func (c *clientConn) Write(p []byte) (n int, err error) {
	if c.hashConn != nil {
		sum := c.hashConn.Sum()
		c.hashConn = nil
		_, err = bufio.WriteVectorised(c.shadowConn, [][]byte{sum, p})
		if err == nil {
			n = len(p)
		}
		return
	}
	return c.shadowConn.Write(p)
}

func (c *clientConn) WriteVectorised(buffers []*buf.Buffer) error {
	if c.hashConn != nil {
		sum := c.hashConn.Sum()
		c.hashConn = nil
		return c.shadowConn.WriteVectorised(append([]*buf.Buffer{buf.As(sum)}, buffers...))
	}
	return c.shadowConn.WriteVectorised(buffers)
}
