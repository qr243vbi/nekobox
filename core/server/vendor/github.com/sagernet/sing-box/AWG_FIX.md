# AmneziaWG Endpoint DNS Resolution Fix

## Problem

The AWG (AmneziaWG) endpoint implementation in `protocol/awg/endpoint.go` did not properly handle domain name resolution when used with sing-box's FakeIP or standard DNS routing.

### Symptoms

1. **FakeIP not working with AWG endpoint**
   - When traffic was routed through `awg-ep`, FakeIP addresses (198.18.x.x) were not being resolved back to real IPs
   - Error: `cannot marshal DNS message`

2. **Domain resolution failing inside netstack**
   - AWG endpoint using `useIntegratedTun: false` (netstack mode) could not resolve domain names
   - Connections to IP addresses worked, but connections to domains failed

3. **Latency tests failing**
   - Clash API delay tests returned errors
   - SOCKS5 connections to domains failed with error code (1)

### Root Cause

The AWG endpoint struct embedded `*awg.Device`:

```go
type Endpoint struct {
    *awg.Device  // ← Embedded, provides DialContext/ListenPacket
    // ...
}
```

This caused `Endpoint.DialContext()` to inherit from `awg.Device.DialContext()`, which:
1. Did not check if destination was a domain (FQDN)
2. Passed domains directly to netstack's dial function
3. Netstack attempted internal DNS resolution and failed

Meanwhile, the standard WireGuard endpoint (`protocol/wireguard/endpoint.go`) properly handles this:

```go
func (w *Endpoint) DialContext(ctx context.Context, network string, destination M.Socksaddr) (net.Conn, error) {
    if destination.IsFqdn() {
        destinationAddresses, err := w.dnsRouter.Lookup(ctx, destination.Fqdn, adapter.DNSQueryOptions{})
        // ... resolve and dial
    }
    return w.endpoint.DialContext(ctx, network, destination)
}
```

## Solution

### Changes Made

**File: `protocol/awg/endpoint.go`**

1. **Added imports:**
```go
E "github.com/sagernet/sing/common/exceptions"
"github.com/sagernet/sing/service"
```

2. **Initialize dnsRouter in constructor:**
```go
return &Endpoint{
    Device:    dev,
    Adapter:   endpoint.NewAdapterWithDialerOptions(...),
    address:   options.Address,
    router:    router,
    logger:    logger,
    dnsRouter: service.FromContext[adapter.DNSRouter](ctx),  // ← Added
}, nil
```

3. **Added DialContext method that overrides embedded Device method:**
```go
func (e *Endpoint) DialContext(ctx context.Context, network string, destination M.Socksaddr) (net.Conn, error) {
    switch network {
    case N.NetworkTCP:
        e.logger.InfoContext(ctx, "outbound connection to ", destination)
    case N.NetworkUDP:
        e.logger.InfoContext(ctx, "outbound packet connection to ", destination)
    }
    if destination.IsFqdn() {
        destinationAddresses, err := e.dnsRouter.Lookup(ctx, destination.Fqdn, adapter.DNSQueryOptions{})
        if err != nil {
            return nil, err
        }
        return N.DialSerial(ctx, e.Device, network, destination, destinationAddresses)
    } else if !destination.Addr.IsValid() {
        return nil, E.New("invalid destination: ", destination)
    }
    return e.Device.DialContext(ctx, network, destination)
}
```

4. **Added ListenPacket method:**
```go
func (e *Endpoint) ListenPacket(ctx context.Context, destination M.Socksaddr) (net.PacketConn, error) {
    e.logger.InfoContext(ctx, "outbound packet connection to ", destination)
    if destination.IsFqdn() {
        destinationAddresses, err := e.dnsRouter.Lookup(ctx, destination.Fqdn, adapter.DNSQueryOptions{})
        if err != nil {
            return nil, err
        }
        packetConn, _, err := N.ListenSerial(ctx, e.Device, destination, destinationAddresses)
        if err != nil {
            return nil, err
        }
        return packetConn, nil
    }
    return e.Device.ListenPacket(ctx, destination)
}
```

## How It Works Now

### Request Flow with FakeIP

```
1. Application requests: ifconfig.me
              ↓
2. DNS (FakeIP) returns: 198.18.0.68
              ↓
3. TUN inbound receives packet to 198.18.0.68:80
              ↓
4. Router checks FakeIP store:
   198.18.0.68 → "ifconfig.me"
              ↓
5. Router sets destination.Fqdn = "ifconfig.me"
              ↓
6. Route rule matches → awg-ep
              ↓
7. AWG Endpoint.DialContext() receives destination with Fqdn
              ↓
8. destination.IsFqdn() == true
              ↓
9. dnsRouter.Lookup("ifconfig.me") → real IP
              ↓
10. Device.DialContext() connects to real IP through tunnel
```

### DNS Configuration

For proper operation with FakeIP, configure DNS rules:

```json
{
  "dns": {
    "servers": [
      {
        "server": "8.8.8.8",
		"type": "tls",
		"detour": "awg-ep"
        "tag": "dns-for-awg-ep"
      },
      {
        "tag": "fakeip-server",
        "type": "fakeip",
        "inet4_range": "198.18.0.0/15"
      }
    ],
    "rules": [
      {
        "outbound": "awg-ep",
        "server": "dns-for-awg-ep"
      },
      {
        "query_type": ["A", "AAAA"],
        "server": "fakeip-server"
      }
    ]
  }
}
```

The rule `"outbound": "awg-ep", "server": "dns-for-awg-ep"` ensures that when AWG endpoint resolves domains, it uses real DNS (not FakeIP).

## Testing

### Before Fix
```bash
$ curl -x socks5h://127.0.0.1:20800 http://ifconfig.me
curl: (97) Can't complete SOCKS5 connection to ifconfig.me. (1)

# Log shows:
# [ERR!] cannot marshal DNS message
```

### After Fix
```bash
$ curl -x socks5h://127.0.0.1:20800 http://ifconfig.me
VPS-IP-returns  # VPN server IP - success!
```

## Build

```bash
go build -tags "with_gvisor,with_quic,with_dhcp,with_wireguard,with_utls,with_acme,with_clash_api,with_awg" ./cmd/sing-box
```

## Compatibility

- This fix aligns AWG endpoint behavior with the standard WireGuard endpoint
- No configuration changes required for existing setups
- FakeIP, standard DNS, and direct IP connections all work correctly
