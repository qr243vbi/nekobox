package process

import (
	"encoding/json"
	"net"
	"strconv"
	"time"

	E "github.com/sagernet/sing/common/exceptions"
)

// LocalInboundAddrs returns the loopback TCP addresses that an Xray-style
// JSON config listens on ("inbounds": [{"listen": "127.0.0.1", "port": N}]).
// Configs it cannot parse yield no addresses.
func LocalInboundAddrs(conf string) []string {
	var parsed struct {
		Inbounds []struct {
			Listen string          `json:"listen"`
			Port   json.RawMessage `json:"port"`
		} `json:"inbounds"`
	}
	if json.Unmarshal([]byte(conf), &parsed) != nil {
		return nil
	}
	var addrs []string
	for _, in := range parsed.Inbounds {
		host := in.Listen
		if host == "" || host == "0.0.0.0" {
			host = "127.0.0.1"
		} else if host == "::" {
			host = "::1"
		}
		ip := net.ParseIP(host)
		if ip == nil || !ip.IsLoopback() {
			continue
		}
		var port int
		if json.Unmarshal(in.Port, &port) != nil {
			var portStr string
			if json.Unmarshal(in.Port, &portStr) != nil {
				continue
			}
			if port, _ = strconv.Atoi(portStr); port == 0 {
				continue
			}
		}
		if port <= 0 || port > 65535 {
			continue
		}
		addrs = append(addrs, net.JoinHostPort(host, strconv.Itoa(port)))
	}
	return addrs
}

// WaitListening blocks until every address accepts a TCP connection, the
// process exits, or the timeout passes. Without this, the GUI starts URL tests
// against the extra core's SOCKS port before it is open and gets
// "connection refused".
func (p *Process) WaitListening(addrs []string, timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for _, addr := range addrs {
		for {
			conn, err := net.DialTimeout("tcp", addr, 200*time.Millisecond)
			if err == nil {
				_ = conn.Close()
				break
			}
			select {
			case <-p.Done():
				return E.New("extra core exited before listening on ", addr)
			default:
			}
			if time.Now().After(deadline) {
				return E.New("extra core is not listening on ", addr, " after ", timeout)
			}
			time.Sleep(100 * time.Millisecond)
		}
	}
	return nil
}
