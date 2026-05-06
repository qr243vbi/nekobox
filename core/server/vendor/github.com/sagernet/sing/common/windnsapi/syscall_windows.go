package windnsapi

//go:generate go run golang.org/x/sys/windows/mkwinsyscall -output zsyscall_windows.go syscall_windows.go

// dnsapi.DnsFlushResolverCache is an undocumented function
//sys FlushResolverCache() (err error) = dnsapi.DnsFlushResolverCache
