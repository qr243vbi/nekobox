# `tfo-go`

[![Go Reference](https://pkg.go.dev/badge/github.com/database64128/tfo-go/v2.svg)](https://pkg.go.dev/github.com/database64128/tfo-go/v2)
[![Test](https://github.com/database64128/tfo-go/actions/workflows/test.yml/badge.svg)](https://github.com/database64128/tfo-go/actions/workflows/test.yml)

`tfo-go` provides TCP Fast Open support for the `net` dialer and listener.

```bash
go get github.com/database64128/tfo-go/v2
```

### Windows support with Go 1.23 and later

`tfo-go`'s Windows support requires extensive usage of `//go:linkname` to access Go runtime internals, as there's currently no public API for Windows async IO in the standard library. Unfortunately, the Go team has decided to [lock down future uses of linkname](https://github.com/golang/go/issues/67401), starting with Go 1.23. And our bid to get the linknames we need exempted was [partially rejected](https://github.com/golang/go/issues/67401#issuecomment-2126175774). Therefore, we had to make the following changes:

- Windows support is gated behind the build tag `tfogo_checklinkname0` when building with Go 1.23 and later.
- With Go 1.21 and 1.22, `tfo-go` v2.2.x still provides full Windows support, with or without the build tag.
- With Go 1.23 and later, when the build tag is not specified, `tfo-go` only supports `listen` with TFO on Windows. To get full TFO support on Windows, the build tag `tfogo_checklinkname0` must be specified along with linker flag `-checklinkname=0` to disable the linkname check.

## License

[MIT](LICENSE)
