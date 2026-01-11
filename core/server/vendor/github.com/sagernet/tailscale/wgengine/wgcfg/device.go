// Copyright (c) Tailscale Inc & AUTHORS
// SPDX-License-Identifier: BSD-3-Clause

package wgcfg

import (
	"context"
	"io"
	"sort"

	"github.com/sagernet/tailscale/types/logger"
	"github.com/sagernet/tailscale/util/multierr"
	"github.com/sagernet/wireguard-go/conn"
	"github.com/sagernet/wireguard-go/device"
	"github.com/sagernet/wireguard-go/tun"
)

// NewDevice returns a wireguard-go Device configured for Tailscale use.
func NewDevice(ctx context.Context, tunDev tun.Device, bind conn.Bind, logger *device.Logger, workers int) *device.Device {
	if ctx == nil {
		ctx = context.Background()
	}
	ret := device.NewDevice(ctx, tunDev, bind, logger, workers)
	ret.DisableSomeRoamingForBrokenMobileSemantics()
	return ret
}

func DeviceConfig(d *device.Device) (*Config, error) {
	r, w := io.Pipe()
	errc := make(chan error, 1)
	go func() {
		errc <- d.IpcGetOperation(w)
		w.Close()
	}()
	cfg, fromErr := FromUAPI(r)
	r.Close()
	getErr := <-errc
	err := multierr.New(getErr, fromErr)
	if err != nil {
		return nil, err
	}
	sort.Slice(cfg.Peers, func(i, j int) bool {
		return cfg.Peers[i].PublicKey.Less(cfg.Peers[j].PublicKey)
	})
	return cfg, nil
}

// ReconfigDevice replaces the existing device configuration with cfg.
func ReconfigDevice(d *device.Device, cfg *Config, logf logger.Logf) (err error) {
	defer func() {
		if err != nil {
			logf("wgcfg.Reconfig failed: %v", err)
		}
	}()

	prev, err := DeviceConfig(d)
	if err != nil {
		return err
	}

	r, w := io.Pipe()
	errc := make(chan error, 1)
	go func() {
		errc <- d.IpcSetOperation(r)
		r.Close()
	}()

	toErr := cfg.ToUAPI(logf, w, prev)
	w.Close()
	setErr := <-errc
	return multierr.New(setErr, toErr)
}
