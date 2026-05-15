//go:build freebsd
package boxdns

import (
	"context"
	"net"
)

type interfaceMonitor struct{}

func newInterfaceMonitor(options *monitorOptions) (*interfaceMonitor, error) {
	return &interfaceMonitor{}, nil
}

func (m *interfaceMonitor) Start() error { return nil }
func (m *interfaceMonitor) Stop() error  { return nil }
func (m *interfaceMonitor) List(ctx context.Context) ([]net.Interface, error) {
	return net.Interfaces()
}
func (m *interfaceMonitor) Interface(ctx context.Context, name string) (*net.Interface, error) {
	return net.InterfaceByName(name)
}
func (m *interfaceMonitor) InterfaceAddrs(ctx context.Context, name string) ([]net.Addr, error) {
	i, e := net.InterfaceByName(name)
	if e != nil {
		return nil, e
	}
	return i.Addrs()
}
func (m *interfaceMonitor) Close() error { return nil }
