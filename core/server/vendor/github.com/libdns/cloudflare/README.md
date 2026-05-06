Cloudflare for `libdns`
=======================

[![godoc reference](https://img.shields.io/badge/godoc-reference-blue.svg)](https://pkg.go.dev/github.com/libdns/cloudflare)

This package implements the [libdns interfaces](https://github.com/libdns/libdns) for [Cloudflare](https://www.cloudflare.com).

## Authenticating

> [!IMPORTANT]
> This package supports API **token** authentication (as opposed to legacy API **keys**).

There are two approaches for token permissions supported by this package: 
1. Single token for everything
    - `APIToken` permissions required: Zone:Read, Zone.DNS:Write - All zones
2. Dual token method
    - `ZoneToken` permissions required: Zone:Read - All zones
    - `APIToken` permissions required: Zone.DNS:Write - for the zone(s) you wish to manage

The dual token method allows users who have multiple DNS zones in their Cloudflare account to restrict which zones the token can access, whereas the first method will allow access to all DNS Zones.
If you only have one domain/zone then this approach does not provide any benefit, and you might as well just have the single API token

To use the dual token approach simply ensure that the `ZoneToken` property is provided - otherwise the package will use `APIToken` for all API requests.

To clarify, do NOT use API keys, which are globally-scoped:

![Don't use API keys](https://user-images.githubusercontent.com/1128849/81196485-556aca00-8f7c-11ea-9e13-c6a8a966f689.png)

DO use scoped API tokens:

![Don't use API keys](https://user-images.githubusercontent.com/1128849/81196503-5c91d800-8f7c-11ea-93cc-ad7d73420fab.png)

## Example Configuration

```golang
// With Auth
p := cloudflare.Provider{
    APIToken: "apitoken",
    ZoneToken: "zonetoken", // optional
}

// With Custom HTTP Client
p := cloudflare.Provider{
    APIToken: "apitoken",
    HTTPClient: http.Client{
        Timeout: 10 * time.Second,
    },
}
```
