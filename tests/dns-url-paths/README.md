# Preserve complete HTTPS/H3 DNS paths

A narrow DNS-generation defect found while auditing issue #243. This does not
reproduce the reporter's empty NextDNS statistics: no URL/configuration was
provided, and no live DNS, TLS, HTTP, VPN, core, profile or settings action is
part of these tests.

## Contract and change

`BuildDnsObject` previously split HTTPS/H3 suffixes on every slash and retained
only the final segment. Thus `/tenant/device/dns-query` became `dns-query`, and
`/a/` or `/` omitted the path entirely. The pinned core defaults an omitted/empty
path to `/dns-query`, so these are different endpoints.

Preserve the entire escaped suffix from the first slash, including leading,
repeated and trailing slashes. Do not percent-decode it. Remove only the initial
scheme prefix, avoiding corruption of a path containing text such as
`/https://example.invalid/h3://nested`. Host/port splitting, non-HTTP transports,
TLS JSON shape and TUN-flag behavior are unchanged. No new URL parser, storage
migration or core change is introduced.

The original one-segment path still targets the same endpoint: the core's URL
serialization adds a leading slash even when the old field omitted it. The new
JSON spells that path explicitly; no one-segment transport failure is claimed.

## Pinned source contract

The test copies this checkout's `core/server/go.mod` and `go.sum` into a temporary
module and uses its real replace graph:

- sing-box -> `github.com/shtorm-7/sing-box-extended` v1.14.1-extended-2.7.2
- sing -> `github.com/shtorm-7/sing` v0.9.0-beta.4-extended-1.2.1

The first dependency's `option/dns.go` defines
`RemoteHTTPSDNSServerOptions.Path` and the contextual `DNSServerOptions` decoder.
Both `dns/transport/https.go` and `dns/transport/quic/http3.go` apply the empty-path
`/dns-query` default, then `sHTTP.URLSetPath`. They serialize the URL with
`destination.String()` before creating a request. The second dependency's
`protocol/http/link.go` links that helper to Go `net/url.(*URL).setPath`, which
preserves valid escaped path bytes through `Path`/`RawPath`. Tests call only the
options decoder and that pure URL helper, then parse the serialized URL in memory.
The pure URL check uses a synthetic host and verifies the path/request URI;
it does not construct the actual authority or TLS settings. Generated server
and port fields are checked separately by the Qt and real option decoder.
No transport constructor or HTTP request is created or sent.

## Deliberate limits

Literal query/fragment components remain unsupported by the existing core
path-only option. For example `/a/b?x=1` is encoded as `/a/b%3Fx=1`, not sent with
an actual query, and `#note` is encoded as a path character. The characterization
rows make this limitation visible; they do not claim complete URL preservation
for these inputs. The patch does not silently strip components or implement a
query feature. Escaped `%3F`/`%23` are legitimate path bytes and are preserved.
Malformed percent escapes remain available to normal core validation; the inert
helper confirms they are rejected. Other malformed URLs, userinfo, URL-wide
validation and no-path query/fragment forms are outside this patch.

The full-config DNS normalizer's choice to rewrite servers to the app's remote
DNS is a separate policy question and is untouched. These tests do not execute
whole configuration selection, resolver/default/tag routing, core startup or
transport/network behavior. The separate Rust TUI DNS builder remains outside
this GUI C++ patch.

## Run

Requires Python 3, C++20, pkg-config and native Qt6Core development files:

```sh
python3 tests/dns-url-paths/run_test.py
python3 tests/dns-url-paths/run_test.py --sanitize
python3 tests/dns-url-paths/mutation_checks.py
python3 tests/test_dns_address.py
```

For the pinned options/URL boundary, supply Go at least as new as the module's
Go directive and an already populated dependency cache:

```sh
GOMODCACHE=/path/to/modules GOCACHE=/path/to/cache \
  python3 tests/dns-url-paths/run_test.py --core --go /path/to/go
```

The runner forces `GOPROXY=off`, `GOSUMDB=off`, `GOTOOLCHAIN=local`, `GOWORK=off`,
`CGO_ENABLED=0` and `-mod=readonly`; it never downloads missing dependencies.
`--source FILE` reads an unchanged baseline or temporary mutation for a red check.
`--repo PATH` selects the source and matching pinned Go module files.

## Coverage and limitations

The native fixture compiles the complete production `BuildDnsObject` verbatim,
using real Qt strings and JSON. Its 324 cases comprise 300 supported-path cases,
12 unsupported literal-query/fragment characterizations and 12 invalid-escape
controls. They cover HTTPS/H3, conventional and legacy semicolon ports, IPv6,
empty/single/multiple/repeated/trailing/root paths, encoded slash/question/hash,
double encoding, escaped UTF-8, spaces, plus signs and scheme-like path text,
each with both TUN flags. The Go check decodes every generated object using real
pinned options, checks the resulting URL path, independently compares supported
standard URL inputs, and rejects four malformed schema controls.

The existing 774-case DNS address suite retains its host/port/local/DHCP/DoT/
UDP/TCP/QUIC and invalid-port controls with corrected HTTP path expectations.
Eight mutations prove the new native fixture rejects per-transport truncation,
per-transport global scheme replacement, discarded root/trailing/leading slashes
and premature encoded-slash decoding. Passing this scoped fixture is not a full
application build or unrestricted repository-suite pass.
