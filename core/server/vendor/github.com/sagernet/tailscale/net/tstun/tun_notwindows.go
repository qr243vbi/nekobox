// Copyright (c) Tailscale Inc & AUTHORS
// SPDX-License-Identifier: BSD-3-Clause

//go:build !windows

package tstun

import "github.com/sagernet/wireguard-go/tun"

func interfaceName(dev tun.Device) (string, error) {
	return dev.Name()
}
