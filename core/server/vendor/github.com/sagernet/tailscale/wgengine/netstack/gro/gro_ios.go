// Copyright (c) Tailscale Inc & AUTHORS
// SPDX-License-Identifier: BSD-3-Clause

//go:build ios

package gro

import (
	"github.com/sagernet/gvisor/pkg/tcpip/stack"
	"github.com/sagernet/tailscale/net/packet"
)

type GRO struct{}

func NewGRO() *GRO {
	panic("unsupported on iOS")
}

func (g *GRO) SetDispatcher(_ stack.NetworkDispatcher) {}

func (g *GRO) Enqueue(_ *packet.Parsed) {}

func (g *GRO) Flush() {}
