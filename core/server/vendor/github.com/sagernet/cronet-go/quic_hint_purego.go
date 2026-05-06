//go:build with_purego

package cronet

import (
	"github.com/sagernet/cronet-go/internal/cronet"
)

func NewQuicHint() QuicHint {
	return QuicHint{cronet.QuicHintCreate()}
}

func (q QuicHint) Destroy() {
	cronet.QuicHintDestroy(q.ptr)
}

func (q QuicHint) SetHost(host string) {
	cronet.QuicHintHostSet(q.ptr, host)
}

func (q QuicHint) Host() string {
	return cronet.QuicHintHostGet(q.ptr)
}

func (q QuicHint) SetPort(port int32) {
	cronet.QuicHintPortSet(q.ptr, port)
}

func (q QuicHint) Port() int32 {
	return cronet.QuicHintPortGet(q.ptr)
}

func (q QuicHint) SetAlternatePort(port int32) {
	cronet.QuicHintAlternatePortSet(q.ptr, port)
}

func (q QuicHint) AlternatePort() int32 {
	return cronet.QuicHintAlternatePortGet(q.ptr)
}
