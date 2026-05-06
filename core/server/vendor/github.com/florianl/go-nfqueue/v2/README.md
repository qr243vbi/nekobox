go-nfqueue [![PkgGoDev](https://pkg.go.dev/badge/github.com/florianl/go-nfqueue/v2)](https://pkg.go.dev/github.com/florianl/go-nfqueue/v2) [![Go Report Card](https://goreportcard.com/badge/github.com/florianl/go-nfqueue/v2)](https://goreportcard.com/report/github.com/florianl/go-nfqueue/v2)
============

This is `go-nfqueue` and it is written in [golang](https://golang.org/). It provides a [C](https://en.wikipedia.org/wiki/C_(programming_language))-binding free API to the netfilter based queue subsystem of the [Linux kernel](https://www.kernel.org).

## Privileges

This package processes information directly from the kernel and therefore it requires special privileges. You can provide this privileges by adjusting the `CAP_NET_ADMIN` capabilities.
```
	setcap 'cap_net_admin=+ep' /your/executable
```

For documentation and more examples please take a look at [documentation](https://pkg.go.dev/github.com/florianl/go-nfqueue).

## Requirements

* A version of Go that is [supported by upstream](https://golang.org/doc/devel/release.html#policy)
