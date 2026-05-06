AliDNS for [`libdns`](https://github.com/libdns/libdns)
=======================
[![Go Reference](https://pkg.go.dev/badge/test.svg)](https://pkg.go.dev/github.com/libdns/alidns)

This package implements the [libdns interfaces](https://github.com/libdns/libdns), allowing you to manage DNS records with the [AliDNS API](https://api.aliyun.com/document/Alidns/2015-01-09/overview) ( which has a nice Go SDK implementation [here](https://github.com/aliyun/alibabacloud-go-sdk/tree/7f23fa1a357f6570dddd74103a15edeea5b69d37/alidns-20150109) )

The metadata of AliDNS API [here](https://api.aliyun.com/meta/v1/products/Alidns/versions/2015-01-09/api-docs.json).

The document of request and signing processing are [v2](https://help.aliyun.com/zh/sdk/product-overview/rpc-mechanism) and [v3](https://help.aliyun.com/zh/sdk/product-overview/v3-request-structure-and-signature) (Current schema version is already v3, v2 will be archived. ).

## Authenticating

To authenticate you need to supply our AccessKeyId and AccessKeySecret to the Provider.

## Example

Here's a minimal example of how to get all your DNS records using this `libdns` provider

```go
package main

import (
        "context"
        "fmt"
        "github.com/libdns/alidns"
)

func main() {
        provider := alidns.Provider{
               AccessKeyID: "<AccessKeyId form your aliyun console>",
               AccessKeySecret: "<AccessKeySecret form your aliyun console>",
        }

        records, err  := provider.GetRecords(context.TODO(), "example.com")
        if err != nil {
                fmt.Println(err.Error())
        }

        for _, record := range records {
                tmp := record.RR()
                fmt.Printf("%s %v %s %s\n", tmp.Name, tmp.TTL.Seconds(), tmp.Type, tmp.Value)
        }
}
```
For complete demo check [_demo/demo.go](_demo/demo.go)
