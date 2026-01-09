// Copyright (c) Tailscale Inc & AUTHORS
// SPDX-License-Identifier: BSD-3-Clause

//go:build !linux

package ethtool

import (
	"runtime"

	"github.com/sagernet/tailscale/types/logger"
)

func ethtoolImpl(logf logger.Logf) error {
	logf("unsupported on %s/%s", runtime.GOOS, runtime.GOARCH)
	return nil
}
