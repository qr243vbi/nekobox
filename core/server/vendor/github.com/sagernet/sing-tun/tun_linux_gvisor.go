//go:build with_gvisor && linux

package tun

import (
	"github.com/sagernet/gvisor/pkg/rawfile"
	"github.com/sagernet/gvisor/pkg/tcpip/link/fdbased"
	"github.com/sagernet/gvisor/pkg/tcpip/stack"

	"golang.org/x/sys/unix"
)

func init() {
	fdbased.BufConfig = []int{65535}
}

var _ GVisorTun = (*NativeTun)(nil)

func (t *NativeTun) WritePacket(pkt *stack.PacketBuffer) (int, error) {
	iovecs := t.iovecsOutputDefault
	var dataLen int
	for _, packetSlice := range pkt.AsSlices() {
		dataLen += len(packetSlice)
		iovec := unix.Iovec{
			Base: &packetSlice[0],
		}
		iovec.SetLen(len(packetSlice))
		iovecs = append(iovecs, iovec)
	}
	if cap(iovecs) > cap(t.iovecsOutputDefault) {
		t.iovecsOutputDefault = iovecs[:0]
	}
	errno := rawfile.NonBlockingWriteIovec(t.tunFd, iovecs)
	if errno == 0 {
		return dataLen, nil
	} else {
		return 0, errno
	}
}

func (t *NativeTun) NewEndpoint() (stack.LinkEndpoint, stack.NICOptions, error) {
	if t.vnetHdr {
		ep, err := fdbased.New(&fdbased.Options{
			FDs:               []int{t.tunFd},
			MTU:               t.options.MTU,
			GSOMaxSize:        gsoMaxSize,
			GRO:               true,
			RXChecksumOffload: true,
			TXChecksumOffload: t.txChecksumOffload,
		})
		if err != nil {
			return nil, stack.NICOptions{}, err
		}
		return ep, stack.NICOptions{}, nil
	} else {
		ep, err := fdbased.New(&fdbased.Options{
			FDs:               []int{t.tunFd},
			MTU:               t.options.MTU,
			RXChecksumOffload: true,
			TXChecksumOffload: t.txChecksumOffload,
		})
		if err != nil {
			return nil, stack.NICOptions{}, err
		}
		return ep, stack.NICOptions{}, nil
	}
}
