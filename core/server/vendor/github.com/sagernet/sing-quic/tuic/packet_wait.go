package tuic

import (
	"io"
	"os"

	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

func (c *udpPacketConn) InitializeReadWaiter(options N.ReadWaitOptions) (needCopy bool) {
	c.readWaitOptions = options
	return options.NeedHeadroom()
}

func (c *udpPacketConn) WaitReadPacket() (buffer *buf.Buffer, destination M.Socksaddr, err error) {
	select {
	case p := <-c.data:
		destination = p.destination
		if c.readWaitOptions.NeedHeadroom() {
			buffer = c.readWaitOptions.NewPacketBuffer()
			p.releaseMessage()
			_, err = buffer.Write(p.data.Bytes())
			if err != nil {
				buffer.Release()
				return
			}
			c.readWaitOptions.PostReturn(buffer)
		} else {
			buffer = p.data
			p.release()
		}
		return
	case <-c.ctx.Done():
		return nil, M.Socksaddr{}, io.ErrClosedPipe
	case <-c.readDeadline.Wait():
		return nil, M.Socksaddr{}, os.ErrDeadlineExceeded
	}
}
