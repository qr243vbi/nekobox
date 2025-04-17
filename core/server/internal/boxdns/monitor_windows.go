package boxdns

import (
	"encoding/binary"
	"fmt"
	"github.com/sagernet/sing/common/control"
	logger2 "github.com/sagernet/sing/common/logger"
	"log"
	"nekobox_core/internal/boxdns/winipcfg"
	"strings"

	tun "github.com/sagernet/sing-tun"

	"github.com/gofrs/uuid/v5"
	"golang.org/x/sys/windows/registry"
)

func init() {
	logger := logger2.NOP()
	updMonitor, err := tun.NewNetworkUpdateMonitor(logger)
	if err != nil {
		fmt.Println("Could not create NetworkUpdateMonitor")
		return
	}
	monitor, err := tun.NewDefaultInterfaceMonitor(updMonitor, logger, tun.DefaultInterfaceMonitorOptions{
		InterfaceFinder: control.NewDefaultInterfaceFinder(),
	})
	if err != nil {
		fmt.Println("Could not create DefaultInterfaceMonitor")
		return
	}
	DnsManagerInstance = &DnsManager{Monitor: monitor}
	monitor.RegisterCallback(DnsManagerInstance.HandleUnderlyingDNS)
	monitor.RegisterCallback(DnsManagerInstance.HandleSystemDNS)
	if err = updMonitor.Start(); err != nil {
		fmt.Println("Could not start updMonitor")
		return
	}
	if err = monitor.Start(); err != nil {
		fmt.Println("Could not start monitor")
		return
	}

	//DnsManagerInstance.HandleUnderlyingDNS(monitor.DefaultInterface(), 0)
}

var DnsManagerInstance *DnsManager

type DnsManager struct {
	Monitor tun.DefaultInterfaceMonitor
	lastIfc control.Interface
}

func (d *DnsManager) HandleUnderlyingDNS(ifc *control.Interface, flag int) {
	if d == nil {
		fmt.Println("No DnsManager, you may need to restart nekoray")
		return
	}
	if ifc == nil {
		log.Println("Default interface is nil!")
		return
	}
	index := ifc.Index
	u, err := ifcIdxtoUUID(index)
	if err != nil {
		log.Println("Failed to get uuid for interface")
		return
	}
	guidStr := "{" + u.String() + "}"
	dns := getFirstDNS(guidStr)
	if len(customDNS) > 0 && dns == customDNS[0].String() {
		log.Println("Interface DNS is the same as Hijack dns, You may need to fix it manually!")
		return
	}
	if dns != "" && dns != underlyingDNS {
		underlyingDNS = dns
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

func ifcIdxtoUUID(index int) (*uuid.UUID, error) {
	luid, err := winipcfg.LUIDFromIndex(uint32(index))
	if err != nil {
		log.Println("Could not get luid from index")
		return nil, err
	}
	guid, err := luid.GUID()
	if err != nil {
		log.Println("Could not get guid from luid")
		return nil, err
	}
	data1 := make([]byte, 4)
	data2 := make([]byte, 2)
	data3 := make([]byte, 2)
	binary.LittleEndian.PutUint32(data1, guid.Data1)
	binary.LittleEndian.PutUint16(data2, guid.Data2)
	binary.LittleEndian.PutUint16(data3, guid.Data3)
	u, _ := uuid.FromBytes([]byte{
		data1[3], data1[2], data1[1], data1[0],
		data2[1], data2[0],
		data3[1], data3[0],
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7],
	})
	return &u, nil
}
