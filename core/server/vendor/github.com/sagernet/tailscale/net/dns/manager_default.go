// Copyright (c) Tailscale Inc & AUTHORS
// SPDX-License-Identifier: BSD-3-Clause

//go:build !linux && !freebsd && !openbsd && !windows && !darwin && !illumos && !solaris

package dns

import (
	"github.com/sagernet/tailscale/control/controlknobs"
	"github.com/sagernet/tailscale/health"
	"github.com/sagernet/tailscale/types/logger"
)

// NewOSConfigurator creates a new OS configurator.
//
// The health tracker and the knobs may be nil and are ignored on this platform.
func NewOSConfigurator(logger.Logf, *health.Tracker, *controlknobs.Knobs, string) (OSConfigurator, error) {
	return NewNoopManager()
}
