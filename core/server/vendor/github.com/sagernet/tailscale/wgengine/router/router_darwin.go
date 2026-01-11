// Copyright (c) Tailscale Inc & AUTHORS
// SPDX-License-Identifier: BSD-3-Clause

package router

import (
	"github.com/sagernet/tailscale/health"
	"github.com/sagernet/tailscale/net/netmon"
	"github.com/sagernet/tailscale/types/logger"
	"github.com/sagernet/wireguard-go/tun"
)

func newUserspaceRouter(logf logger.Logf, tundev tun.Device, netMon *netmon.Monitor, health *health.Tracker) (Router, error) {
	return newUserspaceBSDRouter(logf, tundev, netMon, health)
}

func cleanUp(logger.Logf, string) {
	// Nothing to do.
}
