package main

import (
	"context"
	"errors"
	"fmt"
	"github.com/sagernet/sing-box/common/settings"
	"github.com/sagernet/sing/common/metadata"
	"strings"
	"time"

	"grpc_server"
	"grpc_server/gen"

	"github.com/Mahdi-zarei/sing-box-extra/boxapi"
	"github.com/Mahdi-zarei/sing-box-extra/boxbox"
	"github.com/Mahdi-zarei/sing-box-extra/boxmain"
	"github.com/matsuridayo/libneko/neko_common"
	"github.com/matsuridayo/libneko/neko_log"
	"github.com/matsuridayo/libneko/speedtest"

	"log"

	"github.com/sagernet/sing-box/option"
)

type server struct {
	grpc_server.BaseServer
}

func (s *server) Start(ctx context.Context, in *gen.LoadConfigReq) (out *gen.ErrorResp, _ error) {
	var err error

	defer func() {
		out = &gen.ErrorResp{}
		if err != nil {
			out.Error = err.Error()
			instance = nil
		}
	}()

	if neko_common.Debug {
		log.Println("Start:", in.CoreConfig)
	}

	if instance != nil {
		err = errors.New("instance already started")
		return
	}

	instance, instance_cancel, err = boxmain.Create([]byte(in.CoreConfig), false)

	if instance != nil {
		// Logger
		instance.SetLogWritter(neko_log.LogWriter)
		// V2ray Service
		if in.StatsOutbounds != nil && !in.DisableStats {
			instance.Router().SetV2RayServer(boxapi.NewSbV2rayServer(option.V2RayStatsServiceOptions{
				Enabled:   true,
				Outbounds: in.StatsOutbounds,
			}))
		}
	}

	return
}

func (s *server) Stop(ctx context.Context, in *gen.EmptyReq) (out *gen.ErrorResp, _ error) {
	var err error

	defer func() {
		out = &gen.ErrorResp{}
		if err != nil {
			out.Error = err.Error()
		}
	}()

	if instance == nil {
		return
	}

	instance.CloseWithTimeout(instance_cancel, time.Second*2, log.Println)

	instance = nil

	return
}

func (s *server) Test(ctx context.Context, in *gen.TestReq) (out *gen.TestResp, _ error) {
	var err error
	out = &gen.TestResp{Ms: 0}

	defer func() {
		if err != nil {
			out.Error = err.Error()
		}
	}()

	if in.Mode == gen.TestMode_UrlTest {
		var i *boxbox.Box
		var cancel context.CancelFunc
		if in.Config != nil {
			// Test instance
			i, cancel, err = boxmain.Create([]byte(in.Config.CoreConfig), true)
			if i != nil {
				defer i.Close()
				defer cancel()
			}
			if err != nil {
				return
			}
		} else {
			// Test running instance
			i = instance
			if i == nil {
				return
			}
		}
		// Latency
		out.Ms, err = speedtest.UrlTest(boxapi.CreateProxyHttpClient(i), in.Url, in.Timeout)
	} else if in.Mode == gen.TestMode_TcpPing {
		out.Ms, err = speedtest.TcpPing(in.Address, in.Timeout)
	} else if in.Mode == gen.TestMode_FullTest {
		i, cancel, err := boxmain.Create([]byte(in.Config.CoreConfig), true)
		if i != nil {
			defer i.Close()
			defer cancel()
		}
		if err != nil {
			return
		}
		return grpc_server.DoFullTest(ctx, in, i)
	}

	return
}

func (s *server) QueryStats(ctx context.Context, in *gen.QueryStatsReq) (out *gen.QueryStatsResp, _ error) {
	out = &gen.QueryStatsResp{}

	if instance != nil && instance.Router().V2RayServer() != nil {
		if ss, ok := instance.Router().V2RayServer().(*boxapi.SbV2rayServer); ok {
			out.Traffic = ss.QueryStats(fmt.Sprintf("outbound>>>%s>>>traffic>>>%s", in.Tag, in.Direct))
		}
	}

	return
}

func (s *server) ListConnections(ctx context.Context, in *gen.EmptyReq) (*gen.ListConnectionsResp, error) {
	out := &gen.ListConnectionsResp{
		// TODO upstream api
	}
	return out, nil
}

func (s *server) GetGeoIPList(ctx context.Context, in *gen.EmptyReq) (*gen.GetGeoIPListResponse, error) {
	resp, err := boxmain.ListGeoip()
	if err != nil {
		return nil, err
	}

	res := make([]string, 0)
	for _, r := range resp {
		r += "_IP"
		res = append(res, r)
	}

	return &gen.GetGeoIPListResponse{Items: res}, nil
}

func (s *server) GetGeoSiteList(ctx context.Context, in *gen.EmptyReq) (*gen.GetGeoSiteListResponse, error) {
	resp, err := boxmain.GeositeList()
	if err != nil {
		return nil, err
	}

	res := make([]string, 0)
	for _, r := range resp {
		r += "_SITE"
		res = append(res, r)
	}

	return &gen.GetGeoSiteListResponse{Items: res}, nil
}

func (s *server) CompileGeoIPToSrs(ctx context.Context, in *gen.CompileGeoIPToSrsRequest) (*gen.EmptyResp, error) {
	category := strings.TrimSuffix(in.Item, "_IP")
	err := boxmain.CompileRuleSet(category, boxmain.IpRuleSet, "./rule_sets/"+in.Item+".srs")
	if err != nil {
		return nil, err
	}

	return &gen.EmptyResp{}, nil
}

func (s *server) CompileGeoSiteToSrs(ctx context.Context, in *gen.CompileGeoSiteToSrsRequest) (*gen.EmptyResp, error) {
	category := strings.TrimSuffix(in.Item, "_SITE")
	err := boxmain.CompileRuleSet(category, boxmain.SiteRuleSet, "./rule_sets/"+in.Item+".srs")
	if err != nil {
		return nil, err
	}

	return &gen.EmptyResp{}, nil
}

func (s *server) SetSystemProxy(ctx context.Context, in *gen.SetSystemProxyRequest) (*gen.EmptyResp, error) {
	var err error
	addr := metadata.ParseSocksaddr(in.Address)
	if systemProxyController == nil || systemProxyAddr.String() != addr.String() {
		systemProxyController, err = settings.NewSystemProxy(context.Background(), addr, true)
		if err != nil {
			return nil, err
		}
		systemProxyAddr = addr
	}
	if in.Enable && !systemProxyController.IsEnabled() {
		err = systemProxyController.Enable()
	}
	if !in.Enable && systemProxyController.IsEnabled() {
		err = systemProxyController.Disable()
	}
	if err != nil {
		return nil, err
	}

	return &gen.EmptyResp{}, nil
}
