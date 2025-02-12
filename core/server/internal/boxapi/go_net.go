package boxapi

import (
	"context"
	"github.com/sagernet/sing-box/common/dialer"
	"github.com/sagernet/sing/common/metadata"
	"nekobox_core/internal/boxbox"
	"net"
)

func DialContext(ctx context.Context, box *boxbox.Box, network, addr string) (net.Conn, error) {
	conn, err := dialer.NewDefaultOutbound(box.Outbound()).DialContext(ctx, network, metadata.ParseSocksaddr(addr))
	if err != nil {
		return nil, err
	}
	return conn, nil
}
