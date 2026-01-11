package tun

import (
	"context"

	"github.com/sagernet/sing/common/control"
	"github.com/sagernet/sing/common/logger"
	N "github.com/sagernet/sing/common/network"

	"go4.org/netipx"
)

const (
	DefaultAutoRedirectInputMark  = 0x2023
	DefaultAutoRedirectOutputMark = 0x2024
)

type AutoRedirect interface {
	Start() error
	Close() error
	UpdateRouteAddressSet()
}

type AutoRedirectOptions struct {
	TunOptions             *Options
	Context                context.Context
	Handler                N.TCPConnectionHandlerEx
	Logger                 logger.Logger
	NetworkMonitor         NetworkUpdateMonitor
	InterfaceFinder        control.InterfaceFinder
	TableName              string
	DisableNFTables        bool
	CustomRedirectPort     func() int
	RouteAddressSet        *[]*netipx.IPSet
	RouteExcludeAddressSet *[]*netipx.IPSet
}
