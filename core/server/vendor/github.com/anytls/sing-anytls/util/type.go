package util

import (
	"context"
	"net"
)

type DialOutFunc func(context.Context) (net.Conn, error)
