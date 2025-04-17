package main

import (
	"context"
	"nekobox_core/gen"
	"nekobox_core/internal/boxdns"
	"net/netip"
)

func (s *server) GetDNSDHCPStatus(ctx context.Context, in *gen.EmptyReq) (*gen.GetDNSDHCPStatusResponse, error) {
	dhcp, err := boxdns.DnsManagerInstance.DefaultIfcIsDHCP()
	if err != nil {
		return nil, err
	}

	return &gen.GetDNSDHCPStatusResponse{
		IsDhcp: dhcp,
	}, nil
}

func (s *server) SetSystemDNS(ctx context.Context, in *gen.SetSystemDNSRequest) (*gen.EmptyResp, error) {
	customNS, err := netip.ParseAddr(in.CustomNs)
	if err != nil {
		return nil, err
	}
	err = boxdns.DnsManagerInstance.SetDefaultDNS(customNS, in.SetDhcp, in.Clear)
	if err != nil {
		return nil, err
	}

	return &gen.EmptyResp{}, nil
}
