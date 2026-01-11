package netstack

import "github.com/sagernet/gvisor/pkg/tcpip/stack"

func (ns *Impl) ExportIPStack() *stack.Stack {
	return ns.ipstack
}
