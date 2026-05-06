// Copyright (c) Tailscale Inc & AUTHORS
// SPDX-License-Identifier: BSD-3-Clause

//go:build ts_omit_tailnetlock

package ipnlocal

import (
	"github.com/sagernet/tailscale/ipn"
	"github.com/sagernet/tailscale/ipn/ipnstate"
	"github.com/sagernet/tailscale/tka"
	"github.com/sagernet/tailscale/types/netmap"
)

type tkaState struct {
	authority *tka.Authority
}

func (b *LocalBackend) initTKALocked() error {
	return nil
}

func (b *LocalBackend) tkaSyncIfNeeded(nm *netmap.NetworkMap, prefs ipn.PrefsView) error {
	return nil
}

func (b *LocalBackend) tkaFilterNetmapLocked(nm *netmap.NetworkMap) {}

func (b *LocalBackend) NetworkLockStatus() *ipnstate.NetworkLockStatus {
	return &ipnstate.NetworkLockStatus{Enabled: false}
}
