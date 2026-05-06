package tun

import (
	"net"
	"net/netip"
	"sync"
	"time"

	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/logger"
	"github.com/sagernet/sing/common/x/list"
	"golang.org/x/net/route"
	"golang.org/x/sys/unix"
)

var _ NetworkUpdateMonitor = (*networkUpdateMonitor)(nil)

type networkUpdateMonitor struct {
	access    sync.Mutex
	callbacks list.List[NetworkUpdateCallback]

	closeOnce sync.Once
	done      chan struct{}
	logger    logger.Logger
}

func NewNetworkUpdateMonitor(logger logger.Logger) (NetworkUpdateMonitor, error) {

	return &networkUpdateMonitor{
		logger: logger,
		done:   make(chan struct{}),
	}, nil
}

// Close implements NetworkUpdateMonitor.
func (m *networkUpdateMonitor) Close() error {
	m.closeOnce.Do(func() {
		close(m.done)
	})
	return nil
}

// Start implements NetworkUpdateMonitor.
func (m *networkUpdateMonitor) Start() error {
	go m.loopUpdate()
	return nil
}

func (m *networkUpdateMonitor) loopUpdate() {

	useSocket(unix.AF_ROUTE, unix.SOCK_RAW|unix.SOCK_CLOEXEC, unix.AF_UNSPEC, func(socketFd int) error {

		for {
			select {
			case <-m.done:
				return nil
			case <-time.After(time.Second):
			}
			err := m.updater(socketFd)
			if err != nil {
				m.logger.Error("listen network update: ", err)
				return nil
			}
		}

	})

}

func (m *networkUpdateMonitor) updater(socketFd int) error {
	buffer := buf.NewPacket()
	defer buffer.Release()

	n, err := unix.Read(socketFd, buffer.FreeBytes())
	if err != nil {
		return err
	}
	buffer.Truncate(n)

	messages, err := route.ParseRIB(route.RIBTypeRoute, buffer.Bytes())
	if err != nil {
		return err
	}

	for _, message := range messages {
		if _, isRouteMessage := message.(*route.RouteMessage); isRouteMessage {
			m.emit()
			return nil
		}
	}
	return nil
}

// checkUpdate find the first ipv4 default gateway, then emit event
func (m *defaultInterfaceMonitor) checkUpdate() error {
	// TODO: ipv4 and ipv6 unix.AF_UNSPEC
	ribMessage, err := route.FetchRIB(unix.AF_INET, route.RIBTypeRoute, 0)
	if err != nil {
		return err
	}
	routeMessages, err := route.ParseRIB(route.RIBTypeRoute, ribMessage)
	if err != nil {
		return err
	}

	for _, rawRouteMessage := range routeMessages {
		routeMessage := rawRouteMessage.(*route.RouteMessage)

		// TODO: ipv6
		// switch addr := routeMessage.Addrs[unix.AF_UNSPEC].(type) {
		// case *route.Inet4Addr:

		// case *route.Inet6Addr:

		// }

		// dst addr of this route should be 0.0.0.0
		if destination, isIPv4Destination := routeMessage.Addrs[unix.RTAX_DST].(*route.Inet4Addr); !isIPv4Destination || destination.IP != netip.IPv4Unspecified().As4() {
			continue
		}

		// netmask should be vaild ipv4 addr
		if mask, isIPv4Mask := routeMessage.Addrs[unix.RTAX_NETMASK].(*route.Inet4Addr); !isIPv4Mask {
			continue
		} else {
			// netmask should be 0.0.0.0
			if ones, _ := net.IPMask(mask.IP[:]).Size(); ones != 0 {
				continue
			}
		}

		// the route should be enabled && gateway && static
		flag := unix.RTF_UP | unix.RTF_GATEWAY | unix.RTF_STATIC
		if routeMessage.Flags&(flag) != flag {
			continue
		}

		// the interface of above route should not be loop dev
		if routeInterface, err := net.InterfaceByIndex(routeMessage.Index); err != nil || routeInterface.Flags&net.FlagLoopback != 0 {
			continue
		} else {

			if routeInterface.Name == m.defaultInterfaceName && routeInterface.Index == m.defaultInterfaceIndex {
				return nil
			}

			// update default interface
			m.defaultInterfaceName = routeInterface.Name
			m.defaultInterfaceIndex = routeInterface.Index
			m.emit(EventInterfaceUpdate)

			return nil
		}
	}

	if m.options.UnderNetworkExtension {
		// TODO: fallback of get default interface
		m.logger.Warn("Not implemented: UnderNetworkExtension")
		// defaultInterface, err = getDefaultInterfaceBySocket()
		// if err != nil {
		// 	return err
		// }
	}

	return ErrNoRoute
}
