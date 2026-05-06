// Copyright (c) Tailscale Inc & AUTHORS
// SPDX-License-Identifier: BSD-3-Clause

//go:build ts_omit_debug

package wgengine

import (
	"github.com/sagernet/tailscale/net/packet"
	"github.com/sagernet/tailscale/net/tstun"
	"github.com/sagernet/tailscale/wgengine/filter"
)

type flowtrackTuple = struct{}

type pendingOpenFlow struct{}

func (*userspaceEngine) trackOpenPreFilterIn(pp *packet.Parsed, t *tstun.Wrapper) (res filter.Response) {
	panic("unreachable")
}

func (*userspaceEngine) trackOpenPostFilterOut(pp *packet.Parsed, t *tstun.Wrapper) (res filter.Response) {
	panic("unreachable")
}
