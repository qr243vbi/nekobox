package boxdns

import (
	"fmt"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/control"
	E "github.com/sagernet/sing/common/exceptions"
	"golang.org/x/sys/windows"
	"golang.org/x/sys/windows/registry"
	"log"
	"nekobox_core/internal/boxdns/winipcfg"
	"net/netip"
	"strings"
)

const (
	nameServerRegistryKey = "NameServer"
)

var customns netip.Addr
var dnsIsSet bool

func (d *DnsManager) HandleSystemDNS(ifc *control.Interface, flag int) {
	if d == nil {
		fmt.Println("No DnsManager, you may need to restart nekoray")
		return
	}
	if ifc == nil || ifc.Equals(d.lastIfc) {
		return
	}
	if !dnsIsSet {
		return
	}
	_ = d.SetDefaultDNS(customns, false, false)
}

func (d *DnsManager) getDefaultInterfaceGuid() (string, error) {
	if d.Monitor == nil {
		return "", E.New("No Dns Manager, you may need to restart nekoray")
	}
	ifc := d.Monitor.DefaultInterface()
	if ifc == nil {
		log.Println("Default interface is nil!")
		return "", E.New("Default interface is nil!")
	}
	index := ifc.Index
	u, err := ifcIdxtoUUID(index)
	if err != nil {
		return "", err
	}
	guidStr := "{" + u.String() + "}"

	return guidStr, nil
}

func (d *DnsManager) getDefaultInterfaceLUID() (winipcfg.LUID, error) {
	if d.Monitor == nil {
		return 0, E.New("No DnsManager, you may need to restart nekoray")
	}
	ifc := d.Monitor.DefaultInterface()
	if ifc == nil {
		log.Println("Default interface is nil!")
		return 0, E.New("Default interface is nil!")
	}
	index := ifc.Index
	luid, err := winipcfg.LUIDFromIndex(uint32(index))
	if err != nil {
		return 0, err
	}

	return luid, nil
}

func (d *DnsManager) getNewNameservers(current []netip.Addr, customNS netip.Addr, clear bool) []netip.Addr {
	res := common.Filter(current, func(addr netip.Addr) bool {
		return addr.String() != customNS.String() && addr.Is4()
	})
	if clear {
		return res
	}

	res = append([]netip.Addr{customNS}, res...)
	return res
}

func (d *DnsManager) DefaultIfcIsDHCP() (dhcp bool, err error) {
	if d == nil {
		fmt.Println("No DnsManager, you may need to restart nekoray")
		return false, E.New("No Dns Manager, you may need to restart nekoray")
	}
	guidStr, err := d.getDefaultInterfaceGuid()
	if err != nil {
		return false, err
	}

	key, err := registry.OpenKey(registry.LOCAL_MACHINE, `SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\`+guidStr, registry.QUERY_VALUE)
	if err != nil {
		log.Println("getNameServersForInterface OpenKey:", err)
		return false, err
	}
	defer key.Close()

	if dhcpServers, _, err := key.GetStringValue(nameServerRegistryKey); err == nil {
		if len(strings.TrimSpace(dhcpServers)) > 0 {
			return false, nil
		}
	}

	return true, nil
}

func (d *DnsManager) SetDefaultDNS(customNS netip.Addr, dhcp bool, clear bool) error {
	if d == nil {
		fmt.Println("No DnsManager, you may need to restart nekoray")
		return E.New("No dns Manager, you may need to restart nekoray")
	}
	if clear {
		dnsIsSet = false
	} else {
		customns = customNS
		dnsIsSet = true
		if ifc := d.Monitor.DefaultInterface(); ifc != nil {
			d.lastIfc = *ifc
		}
	}

	luid, err := d.getDefaultInterfaceLUID()
	if err != nil {
		return err
	}

	if dhcp {
		err = luid.FlushDNS(winipcfg.AddressFamily(windows.AF_INET))
		if err != nil {
			return err
		}
		return nil
	}

	servers, err := luid.DNS()
	if err != nil {
		return E.New("Failed to get DNS servers", err)
	}
	servers = d.getNewNameservers(servers, customNS, clear)

	if len(servers) > 0 {
		err = luid.SetDNS(winipcfg.AddressFamily(windows.AF_INET), servers, nil)
		if err != nil {
			return err
		}
	}

	return nil
}
