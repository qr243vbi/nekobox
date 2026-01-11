// Copyright (c) Tailscale Inc & AUTHORS
// SPDX-License-Identifier: BSD-3-Clause

//go:build !windows && !linux && !darwin && !openbsd && !freebsd

package router

import (
	"fmt"
	"runtime"

	"github.com/sagernet/tailscale/health"
	"github.com/sagernet/tailscale/net/netmon"
	"github.com/sagernet/tailscale/types/logger"
	"github.com/sagernet/wireguard-go/tun"
)

func newUserspaceRouter(logf logger.Logf, tunDev tun.Device, netMon *netmon.Monitor, health *health.Tracker) (Router, error) {
	return nil, fmt.Errorf("unsupported OS %q", runtime.GOOS)
}

func cleanUp(logf logger.Logf, interfaceName string) {
	// Nothing to do here.
}
