package control

import (
	"net"
	"net/netip"
	"sync"

	E "github.com/sagernet/sing/common/exceptions"
)

var _ InterfaceFinder = (*DefaultInterfaceFinder)(nil)

type DefaultInterfaceFinder struct {
	access     sync.RWMutex
	interfaces []Interface
}

func NewDefaultInterfaceFinder() *DefaultInterfaceFinder {
	return &DefaultInterfaceFinder{}
}

func (f *DefaultInterfaceFinder) Update() error {
	netIfs, err := net.Interfaces()
	if err != nil {
		return err
	}
	interfaces := make([]Interface, 0, len(netIfs))
	for _, netIf := range netIfs {
		var iif Interface
		iif, err = InterfaceFromNet(netIf)
		if err != nil {
			return err
		}
		interfaces = append(interfaces, iif)
	}
	f.access.Lock()
	f.interfaces = interfaces
	f.access.Unlock()
	return nil
}

func (f *DefaultInterfaceFinder) UpdateInterfaces(interfaces []Interface) {
	f.access.Lock()
	defer f.access.Unlock()
	f.interfaces = interfaces
}

func (f *DefaultInterfaceFinder) Interfaces() []Interface {
	f.access.RLock()
	defer f.access.RUnlock()
	return f.interfaces
}

func (f *DefaultInterfaceFinder) ByName(name string) (*Interface, error) {
	f.access.RLock()
	for _, netInterface := range f.interfaces {
		if netInterface.Name == name {
			f.access.RUnlock()
			return &netInterface, nil
		}
	}
	f.access.RUnlock()
	_, err := net.InterfaceByName(name)
	if err == nil {
		err = f.Update()
		if err != nil {
			return nil, err
		}
		return f.ByName(name)
	}
	return nil, &net.OpError{Op: "route", Net: "ip+net", Source: nil, Addr: &net.IPAddr{IP: nil}, Err: E.New("no such network interface")}
}

func (f *DefaultInterfaceFinder) ByIndex(index int) (*Interface, error) {
	f.access.RLock()
	for _, netInterface := range f.interfaces {
		if netInterface.Index == index {
			f.access.RUnlock()
			return &netInterface, nil
		}
	}
	f.access.RUnlock()
	_, err := net.InterfaceByIndex(index)
	if err == nil {
		err = f.Update()
		if err != nil {
			return nil, err
		}
		return f.ByIndex(index)
	}
	return nil, &net.OpError{Op: "route", Net: "ip+net", Source: nil, Addr: &net.IPAddr{IP: nil}, Err: E.New("no such network interface")}
}

func (f *DefaultInterfaceFinder) ByAddr(addr netip.Addr) (*Interface, error) {
	f.access.RLock()
	defer f.access.RUnlock()
	for _, netInterface := range f.interfaces {
		for _, prefix := range netInterface.Addresses {
			if prefix.Addr() == addr {
				return &netInterface, nil
			}
		}
	}
	for _, netInterface := range f.interfaces {
		for _, prefix := range netInterface.Addresses {
			if prefix.Contains(addr) {
				return &netInterface, nil
			}
		}
	}
	return nil, &net.OpError{Op: "route", Net: "ip+net", Source: nil, Addr: &net.IPAddr{IP: addr.AsSlice()}, Err: E.New("no such network interface")}
}
