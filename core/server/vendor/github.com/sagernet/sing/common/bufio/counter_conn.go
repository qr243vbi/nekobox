package bufio

import (
	"io"
	"net"
	"sync/atomic"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	N "github.com/sagernet/sing/common/network"
)

func NewInt64CounterConn(conn net.Conn, readCounter []*atomic.Int64, writeCounter []*atomic.Int64) *CounterConn {
	return &CounterConn{
		NewExtendedConn(conn),
		common.Map(readCounter, func(it *atomic.Int64) N.CountFunc {
			return func(n int64) {
				it.Add(n)
			}
		}),
		common.Map(writeCounter, func(it *atomic.Int64) N.CountFunc {
			return func(n int64) {
				it.Add(n)
			}
		}),
	}
}

func NewCounterConn(conn net.Conn, readCounter []N.CountFunc, writeCounter []N.CountFunc) *CounterConn {
	return &CounterConn{NewExtendedConn(conn), readCounter, writeCounter}
}

type CounterConn struct {
	N.ExtendedConn
	readCounter  []N.CountFunc
	writeCounter []N.CountFunc
}

func (c *CounterConn) Read(p []byte) (n int, err error) {
	n, err = c.ExtendedConn.Read(p)
	if n > 0 {
		for _, counter := range c.readCounter {
			counter(int64(n))
		}
	}
	return n, err
}

func (c *CounterConn) ReadBuffer(buffer *buf.Buffer) error {
	err := c.ExtendedConn.ReadBuffer(buffer)
	if err != nil {
		return err
	}
	if buffer.Len() > 0 {
		for _, counter := range c.readCounter {
			counter(int64(buffer.Len()))
		}
	}
	return nil
}

func (c *CounterConn) Write(p []byte) (n int, err error) {
	n, err = c.ExtendedConn.Write(p)
	if n > 0 {
		for _, counter := range c.writeCounter {
			counter(int64(n))
		}
	}
	return n, err
}

func (c *CounterConn) WriteBuffer(buffer *buf.Buffer) error {
	dataLen := int64(buffer.Len())
	err := c.ExtendedConn.WriteBuffer(buffer)
	if err != nil {
		return err
	}
	if dataLen > 0 {
		for _, counter := range c.writeCounter {
			counter(dataLen)
		}
	}
	return nil
}

func (c *CounterConn) UnwrapReader() (io.Reader, []N.CountFunc) {
	return c.ExtendedConn, c.readCounter
}

func (c *CounterConn) UnwrapWriter() (io.Writer, []N.CountFunc) {
	return c.ExtendedConn, c.writeCounter
}

func (c *CounterConn) Upstream() any {
	return c.ExtendedConn
}
