// Copyright (c) Tailscale Inc & AUTHORS
// SPDX-License-Identifier: BSD-3-Clause

//go:build with_gvisor

package tun

import "github.com/sagernet/gvisor/pkg/tcpip/transport/tcp"

const (
	// tcp{RX,TX}Buf{Min,Def,Max}Size mirror gVisor defaults. We leave these
	// unchanged on iOS for now as to not increase pressure towards the
	// NetworkExtension memory limit.
	tcpRXBufMinSize = tcp.MinBufferSize
	tcpRXBufDefSize = tcp.DefaultSendBufferSize
	tcpRXBufMaxSize = tcp.MaxBufferSize

	tcpTXBufMinSize = tcp.MinBufferSize
	tcpTXBufDefSize = tcp.DefaultReceiveBufferSize
	tcpTXBufMaxSize = tcp.MaxBufferSize
)
