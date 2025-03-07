package boxdns

import (
	"fmt"
	"github.com/gofrs/uuid/v5"
	"github.com/matsuridayo/libneko/iphlpapi"
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
	dhcpNameServerRegistryKey = "DhcpNameServer"
	nameServerRegistryKey     = "NameServer"
)

var customDNS []netip.Addr
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
	_ = d.SetDefaultDNS(customDNS, false, false)
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
	var guid iphlpapi.GUID
	if errno := iphlpapi.Index2GUID(uint64(index), &guid); errno != 0 {
		return "", E.New("Failed to convert index to GUID")
	}
	u, _ := uuid.FromBytes([]byte{
		guid.Data1[3], guid.Data1[2], guid.Data1[1], guid.Data1[0],
		guid.Data2[1], guid.Data2[0],
		guid.Data3[1], guid.Data3[0],
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7],
	})
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

func (d *DnsManager) GetDefaultDNS() (servers []netip.Addr, dhcp bool, err error) {
	if d == nil {
		fmt.Println("No DnsManager, you may need to restart nekoray")
		return nil, false, E.New("No Dns Manager, you may need to restart nekoray")
	}
	guidStr, err := d.getDefaultInterfaceGuid()
	if err != nil {
		return nil, false, err
	}

	key, err := registry.OpenKey(registry.LOCAL_MACHINE, `SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\`+guidStr, registry.QUERY_VALUE)
	if err != nil {
		log.Println("getNameServersForInterface OpenKey:", err)
		return nil, false, err
	}
	defer key.Close()

	if dhcpServers, _, err := key.GetStringValue(dhcpNameServerRegistryKey); err == nil {
		if len(strings.TrimSpace(dhcpServers)) > 0 {
			return nil, true, nil
		}
	}

	nameServersRaw, _, err := key.GetStringValue(nameServerRegistryKey)
	if err != nil {
		return nil, false, err
	}
	resp := make([]netip.Addr, 0)
	nameServers := strings.Split(strings.ReplaceAll(nameServersRaw, ",", " "), " ")
	for _, server := range nameServers {
		if server != "" {
			addr, _ := netip.ParseAddr(server)
			resp = append(resp, addr)
		}
	}

	return resp, false, nil
}

func (d *DnsManager) SetDefaultDNS(servers []netip.Addr, dhcp bool, clear bool) error {
	if d == nil {
		fmt.Println("No DnsManager, you may need to restart nekoray")
		return E.New("No dns Manager, you may need to restart nekoray")
	}
	if clear {
		dnsIsSet = false
	} else {
		customDNS = servers
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

	hasV4 := false
	for _, server := range servers {
		if server.Is4() {
			hasV4 = true
		}
	}

	if hasV4 {
		err = luid.SetDNS(winipcfg.AddressFamily(windows.AF_INET), servers, nil)
		if err != nil {
			return err
		}
	}

	return nil
}
