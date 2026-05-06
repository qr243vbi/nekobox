// Copyright (c) Tailscale Inc & AUTHORS
// SPDX-License-Identifier: BSD-3-Clause

//go:build aix || solaris || illumos

package tstun

import (
	"github.com/sagernet/tailscale/types/logger"
	"github.com/sagernet/wireguard-go/tun"
)

func New(logf logger.Logf, tunName string) (tun.Device, string, error) {
	panic("not implemented")
}

func Diagnose(logf logger.Logf, tunName string, err error) {
	panic("not implemented")
}
