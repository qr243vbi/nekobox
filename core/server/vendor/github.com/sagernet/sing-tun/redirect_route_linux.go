//go:build linux

package tun

import (
	"math/rand"
	"net"
	"net/netip"

	"github.com/sagernet/netlink"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/control"

	"golang.org/x/sys/unix"
)

const redirectRouteRulePriority = 1

func (r *autoRedirect) setupRedirectRoutes() error {
	for {
		r.redirectRouteTableIndex = int(rand.Uint32())
		if r.redirectRouteTableIndex == r.tunOptions.IPRoute2TableIndex {
			continue
		}
		routeList, fErr := netlink.RouteListFiltered(netlink.FAMILY_ALL,
			&netlink.Route{Table: r.redirectRouteTableIndex},
			netlink.RT_FILTER_TABLE)
		if len(routeList) == 0 || fErr != nil {
			break
		}
	}
	err := r.interfaceFinder.Update()
	if err != nil {
		return E.Cause(err, "update interfaces")
	}
	tunName := r.tunOptions.Name
	r.redirectInterfaces = common.Filter(r.interfaceFinder.Interfaces(), func(it control.Interface) bool {
		return it.Name != "lo" && it.Name != tunName && it.Flags&net.FlagUp != 0
	})
	r.cleanupRedirectRoutes()
	for _, iface := range r.redirectInterfaces {
		err = r.addRedirectRoutes(iface)
		if err != nil {
			return E.Cause(err, "add redirect routes for ", iface.Name)
		}
	}
	if r.enableIPv4 {
		rule := netlink.NewRule()
		rule.Priority = redirectRouteRulePriority
		rule.Table = r.redirectRouteTableIndex
		rule.Family = unix.AF_INET
		err = netlink.RuleAdd(rule)
		if err != nil {
			return E.Cause(err, "add ipv4 redirect rule")
		}
	}
	if r.enableIPv6 {
		rule := netlink.NewRule()
		rule.Priority = redirectRouteRulePriority
		rule.Table = r.redirectRouteTableIndex
		rule.Family = unix.AF_INET6
		err = netlink.RuleAdd(rule)
		if err != nil {
			return E.Cause(err, "add ipv6 redirect rule")
		}
	}
	return nil
}

func (r *autoRedirect) addRedirectRoutes(iface control.Interface) error {
	if r.enableIPv4 && common.Any(iface.Addresses, func(it netip.Prefix) bool {
		return it.Addr().Is4()
	}) {
		err := netlink.RouteAppend(&netlink.Route{
			LinkIndex: iface.Index,
			Dst:       &net.IPNet{IP: net.IPv4(127, 0, 0, 1), Mask: net.CIDRMask(32, 32)},
			Table:     r.redirectRouteTableIndex,
			Type:      unix.RTN_LOCAL,
			Scope:     netlink.SCOPE_HOST,
		})
		if err != nil {
			return E.Cause(err, "append ipv4 loopback route")
		}
	}
	if r.enableIPv6 && common.Any(iface.Addresses, func(it netip.Prefix) bool {
		return it.Addr().Is6() && !it.Addr().Is4In6()
	}) {
		err := netlink.RouteAppend(&netlink.Route{
			LinkIndex: iface.Index,
			Dst:       &net.IPNet{IP: net.IPv6loopback, Mask: net.CIDRMask(128, 128)},
			Table:     r.redirectRouteTableIndex,
			Type:      unix.RTN_LOCAL,
			Scope:     netlink.SCOPE_HOST,
		})
		if err != nil {
			return E.Cause(err, "append ipv6 loopback route")
		}
	}
	return nil
}

func (r *autoRedirect) removeRedirectRoutes(linkIndex int) {
	if r.enableIPv4 {
		_ = netlink.RouteDel(&netlink.Route{
			LinkIndex: linkIndex,
			Dst:       &net.IPNet{IP: net.IPv4(127, 0, 0, 1), Mask: net.CIDRMask(32, 32)},
			Table:     r.redirectRouteTableIndex,
			Type:      unix.RTN_LOCAL,
		})
	}
	if r.enableIPv6 {
		_ = netlink.RouteDel(&netlink.Route{
			LinkIndex: linkIndex,
			Dst:       &net.IPNet{IP: net.IPv6loopback, Mask: net.CIDRMask(128, 128)},
			Table:     r.redirectRouteTableIndex,
			Type:      unix.RTN_LOCAL,
		})
	}
}

func (r *autoRedirect) updateRedirectRoutes() error {
	err := r.interfaceFinder.Update()
	if err != nil {
		return E.Cause(err, "update interfaces")
	}
	tunName := r.tunOptions.Name
	newInterfaces := common.Filter(r.interfaceFinder.Interfaces(), func(it control.Interface) bool {
		return it.Name != "lo" && it.Name != tunName && it.Flags&net.FlagUp != 0
	})
	oldMap := make(map[int]bool, len(r.redirectInterfaces))
	for _, iface := range r.redirectInterfaces {
		oldMap[iface.Index] = true
	}
	newMap := make(map[int]bool, len(newInterfaces))
	for _, iface := range newInterfaces {
		newMap[iface.Index] = true
	}
	for _, iface := range newInterfaces {
		if !oldMap[iface.Index] {
			err = r.addRedirectRoutes(iface)
			if err != nil {
				return E.Cause(err, "add redirect routes for ", iface.Name)
			}
		}
	}
	for _, iface := range r.redirectInterfaces {
		if !newMap[iface.Index] {
			r.removeRedirectRoutes(iface.Index)
		}
	}
	r.redirectInterfaces = newInterfaces
	return nil
}

func (r *autoRedirect) cleanupRedirectRoutes() {
	if r.redirectRouteTableIndex == 0 {
		return
	}
	routes, _ := netlink.RouteListFiltered(netlink.FAMILY_ALL,
		&netlink.Route{Table: r.redirectRouteTableIndex},
		netlink.RT_FILTER_TABLE)
	for _, route := range routes {
		_ = netlink.RouteDel(&route)
	}
	if r.enableIPv4 {
		rule := netlink.NewRule()
		rule.Priority = redirectRouteRulePriority
		rule.Table = r.redirectRouteTableIndex
		rule.Family = unix.AF_INET
		_ = netlink.RuleDel(rule)
	}
	if r.enableIPv6 {
		rule := netlink.NewRule()
		rule.Priority = redirectRouteRulePriority
		rule.Table = r.redirectRouteTableIndex
		rule.Family = unix.AF_INET6
		_ = netlink.RuleDel(rule)
	}
}
