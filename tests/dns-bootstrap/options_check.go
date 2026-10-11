// Decode the regression's real Qt output using the repository-pinned options.
// This deliberately never imports/constructs transports, routers or a core.
package main

import (
	"bufio"
	"context"
	stdjson "encoding/json"
	"fmt"
	"os"

	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/common/json"
	"github.com/sagernet/sing/service"
)

type registry struct{}

func (registry) OptionTypes() []string {
	return []string{"udp", "tcp", "tls", "quic", "https", "h3", "local", "fakeip"}
}
func (registry) CreateOptions(kind string) (any, bool) {
	switch kind {
	case "udp", "tcp":
		return new(option.RemoteDNSServerOptions), true
	case "tls", "quic":
		return new(option.RemoteTLSDNSServerOptions), true
	case "https", "h3":
		return new(option.RemoteHTTPSDNSServerOptions), true
	case "local":
		return new(option.LocalDNSServerOptions), true
	case "fakeip":
		return new(option.FakeIPDNSServerOptions), true
	}
	return nil, false
}

type configuration struct {
	DNS   stdjson.RawMessage `json:"dns"`
	Route stdjson.RawMessage `json:"route"`
}

func remoteOptions(server option.DNSServerOptions) *option.RemoteDNSServerOptions {
	switch value := server.Options.(type) {
	case *option.RemoteDNSServerOptions:
		return value
	case *option.RemoteTLSDNSServerOptions:
		return &value.RemoteDNSServerOptions
	case *option.RemoteHTTPSDNSServerOptions:
		return &value.RemoteDNSServerOptions
	}
	return nil
}

// A test-only reference/acyclicity check complements option decoding, which
// accepts resolver self-references. It is not a core startup or network check.
func validate(ctx context.Context, input configuration) (int, error) {
	var dns option.DNSOptions
	if err := json.UnmarshalContext(ctx, input.DNS, &dns); err != nil {
		return 0, err
	}
	var route option.RouteOptions
	if len(input.Route) != 0 {
		if err := json.UnmarshalContext(ctx, input.Route, &route); err != nil {
			return 0, err
		}
	}
	tags := map[string]bool{}
	dependencies := map[string]string{}
	for _, server := range dns.Servers {
		if server.Tag == "" || tags[server.Tag] {
			return 0, fmt.Errorf("empty or duplicate tag %q", server.Tag)
		}
		tags[server.Tag] = true
		if remote := remoteOptions(server); remote != nil && remote.DomainResolver != nil {
			dependencies[server.Tag] = remote.DomainResolver.Server
		}
	}
	check := func(tag string) error {
		if tag != "" && !tags[tag] {
			return fmt.Errorf("unknown DNS reference %q", tag)
		}
		return nil
	}
	for tag, dependency := range dependencies {
		if err := check(dependency); err != nil {
			return 0, err
		}
		seen := map[string]bool{tag: true}
		for dependency != "" {
			if seen[dependency] {
				return 0, fmt.Errorf("DNS dependency cycle at %q", dependency)
			}
			seen[dependency] = true
			dependency = dependencies[dependency]
		}
	}
	if err := check(dns.Final); err != nil {
		return 0, err
	}
	if route.DefaultDomainResolver != nil {
		if err := check(route.DefaultDomainResolver.Server); err != nil {
			return 0, err
		}
	}
	// Rules are already decoded above; inspect string references recursively so
	// nested logical DNS rules are also checked without constructing a router.
	var dnsRaw map[string]any
	if err := stdjson.Unmarshal(input.DNS, &dnsRaw); err != nil {
		return 0, err
	}
	var checkRules func(any) error
	checkRules = func(value any) error {
		switch value := value.(type) {
		case []any:
			for _, child := range value {
				if err := checkRules(child); err != nil {
					return err
				}
			}
		case map[string]any:
			if tag, ok := value["server"].(string); ok {
				if err := check(tag); err != nil {
					return err
				}
			}
			return checkRules(value["rules"])
		}
		return nil
	}
	if err := checkRules(dnsRaw["rules"]); err != nil {
		return 0, err
	}
	return len(dns.Servers), nil
}

func controls(ctx context.Context) {
	for name, dns := range map[string]string{
		"self-reference":   `{"servers":[{"type":"tls","tag":"one","server":"resolver.invalid","domain_resolver":"one"}]}`,
		"two-node cycle":   `{"servers":[{"type":"tls","tag":"one","server":"resolver.invalid","domain_resolver":"two"},{"type":"tls","tag":"two","server":"resolver.invalid","domain_resolver":"one"}]}`,
		"missing resolver": `{"servers":[{"type":"tls","tag":"one","server":"resolver.invalid","domain_resolver":"missing"}]}`,
		"duplicate tag":    `{"servers":[{"type":"local","tag":"one"},{"type":"local","tag":"one"}]}`,
		"missing final":    `{"servers":[{"type":"local","tag":"one"}],"final":"missing"}`,
		"missing rule":     `{"servers":[{"type":"local","tag":"one"}],"rules":[{"domain":["example.invalid"],"server":"missing"}]}`,
		"invalid schema":   `{"servers":[false]}`,
	} {
		if _, err := validate(ctx, configuration{DNS: stdjson.RawMessage(dns)}); err == nil {
			panic("negative control survived: " + name)
		}
	}
}

func main() {
	ctx := service.ContextWith[option.DNSTransportOptionsRegistry](context.Background(), registry{})
	controls(ctx)
	scanner := bufio.NewScanner(os.Stdin)
	scanner.Buffer(make([]byte, 4096), 4*1024*1024)
	rows, passes, servers, rejected := 0, 0, 0, 0
	for scanner.Scan() {
		var row struct {
			Name   string          `json:"name"`
			Schema bool            `json:"schema"`
			Passes []configuration `json:"passes"`
		}
		if err := stdjson.Unmarshal(scanner.Bytes(), &row); err != nil {
			panic(err)
		}
		for index, value := range row.Passes {
			count, err := validate(ctx, value)
			if row.Schema && err != nil {
				panic(fmt.Sprintf("%s pass %d: %v", row.Name, index+1, err))
			}
			if !row.Schema {
				if err == nil {
					panic("malformed boundary unexpectedly accepted: " + row.Name)
				}
				rejected++
			} else {
				passes++
				servers += count
			}
		}
		rows++
	}
	if err := scanner.Err(); err != nil {
		panic(err)
	}
	if rows == 0 {
		panic("no Qt observations provided")
	}
	fmt.Printf("PASS rows=%d valid_config_passes=%d decoded_servers=%d expected_boundary_rejections=%d negative_controls=7; options/reference checks only, no transport/core startup\n", rows, passes, servers, rejected)
}
