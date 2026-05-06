package netx

import (
	"bufio"
	"os"
	"strings"
)

// Copied and modified from src/net/ipsock_plan9.go

func probe() (p IPStackCapabilities) {
	p.IPv4Enabled = probe2("/net/iproute", "4i")
	p.IPv6Enabled = probe2("/net/iproute", "6i")
	if p.IPv4Enabled && p.IPv6Enabled {
		p.IPv4MappedIPv6Enabled = true
	}
	return
}

func probe2(filename, query string) bool {
	file, err := os.Open(filename)
	if err != nil {
		return false
	}
	defer file.Close()

	r := bufio.NewScanner(file)
	for r.Scan() {
		line := r.Text()
		f := getFields(line)
		if len(f) < 3 {
			continue
		}
		for i := 0; i < len(f); i++ {
			if query == f[i] {
				return true
			}
		}
	}
	return false
}

// Count occurrences in s of any bytes in t.
func countAnyByte(s string, t string) int {
	n := 0
	for i := 0; i < len(s); i++ {
		if strings.IndexByte(t, s[i]) >= 0 {
			n++
		}
	}
	return n
}

// Split s at any bytes in t.
func splitAtBytes(s string, t string) []string {
	a := make([]string, 1+countAnyByte(s, t))
	n := 0
	last := 0
	for i := 0; i < len(s); i++ {
		if strings.IndexByte(t, s[i]) >= 0 {
			if last < i {
				a[n] = s[last:i]
				n++
			}
			last = i + 1
		}
	}
	if last < len(s) {
		a[n] = s[last:]
		n++
	}
	return a[0:n]
}

func getFields(s string) []string { return splitAtBytes(s, " \r\t\n") }
