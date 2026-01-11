// Copyright (c) Tailscale Inc & AUTHORS
// SPDX-License-Identifier: BSD-3-Clause

//go:build (ts_kube || (linux && (arm64 || amd64))) && !ts_omit_kube

package store

import (
	"strings"

	"github.com/sagernet/tailscale/ipn"
	"github.com/sagernet/tailscale/ipn/store/kubestore"
	"github.com/sagernet/tailscale/types/logger"
)

func init() {
	registerAvailableExternalStores = append(registerAvailableExternalStores, registerKubeStore)
}

func registerKubeStore() {
	Register("kube:", func(logf logger.Logf, path string) (ipn.StateStore, error) {
		secretName := strings.TrimPrefix(path, "kube:")
		return kubestore.New(logf, secretName)
	})
}
