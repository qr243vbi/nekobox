package boxdns

import (
	"github.com/matsuridayo/libneko/iphlpapi"
	"log"
	"strings"

	tun "github.com/sagernet/sing-tun"

	"github.com/gofrs/uuid/v5"
	"golang.org/x/sys/windows/registry"
)

var DefaultIfcMonitor tun.DefaultInterfaceMonitor

func monitorForUnderlyingDNS() {
	ifc := DefaultIfcMonitor.DefaultInterface()
	if ifc == nil {
		log.Println("Default interface is nil!")
		return
	}
	index := ifc.Index
	var guid iphlpapi.GUID
	if errno := iphlpapi.Index2GUID(uint64(index), &guid); errno != 0 {
		return
	}
	u, _ := uuid.FromBytes([]byte{
		guid.Data1[3], guid.Data1[2], guid.Data1[1], guid.Data1[0],
		guid.Data2[1], guid.Data2[0],
		guid.Data3[1], guid.Data3[0],
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7],
	})
	guidStr := "{" + u.String() + "}"
	if getFirstDNS(guidStr) != underlyingDNS {
		underlyingDNS = getFirstDNS(guidStr)
		log.Println("underlyingDNS:", guidStr, underlyingDNS)
	}
}

func getFirstDNS(guid string) string {
	dns, err := getNameServersForInterface(guid)
	if err != nil || len(dns) == 0 {
		return ""
	}
	return dns[0]
}

func getNameServersForInterface(guid string) ([]string, error) {
	key, err := registry.OpenKey(registry.LOCAL_MACHINE, `SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\`+guid, registry.QUERY_VALUE)
	if err != nil {
		log.Println("getNameServersForInterface OpenKey:", err)
		return nil, err
	}
	defer key.Close()

	nameservers := make([]string, 0, 4)
	for _, name := range []string{`NameServer`, `DhcpNameServer`} {
		s, _, err := key.GetStringValue(name)
		if err != nil {
			continue
		}
		s = strings.ReplaceAll(s, ",", " ")
		for _, server := range strings.Split(s, " ") {
			if server != "" && server != "127.0.0.1" {
				nameservers = append(nameservers, server)
			}
		}
	}

	if len(nameservers) == 0 {
		log.Println("getNameServersForInterface: no nameservers found")
	}

	return nameservers, nil
}
