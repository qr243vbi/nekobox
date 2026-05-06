//go:build linux
// +build linux

package unix

import (
	linux "golang.org/x/sys/unix"
)

// various constants
const (
	AF_INET           = linux.AF_INET
	AF_INET6          = linux.AF_INET6
	AF_UNSPEC         = linux.AF_UNSPEC
	NFNETLINK_V0      = linux.NFNETLINK_V0
	NETLINK_NETFILTER = linux.NETLINK_NETFILTER
)
