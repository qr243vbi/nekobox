package dnscache

import (
	"context"
	"net/netip"
)

type LookupHookFunc func(ctx context.Context, host string) ([]netip.Addr, error)
