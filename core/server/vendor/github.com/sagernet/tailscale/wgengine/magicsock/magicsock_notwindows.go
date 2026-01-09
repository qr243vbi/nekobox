// Copyright (c) Tailscale Inc & AUTHORS
// SPDX-License-Identifier: BSD-3-Clause

//go:build !windows

package magicsock

import (
	"github.com/sagernet/tailscale/types/logger"
	"github.com/sagernet/tailscale/types/nettype"
)

func trySetUDPSocketOptions(pconn nettype.PacketConn, logf logger.Logf) {}
