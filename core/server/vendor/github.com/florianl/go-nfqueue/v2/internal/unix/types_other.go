//go:build !linux
// +build !linux

package unix

const (
	AF_INET           = 0x2
	AF_INET6          = 0xa
	AF_UNSPEC         = 0x0
	NFNETLINK_V0      = 0x0
	NETLINK_NETFILTER = 0xc
)
