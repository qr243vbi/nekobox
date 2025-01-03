package main

import (
	"context"
	"errors"
	"fmt"
	"github.com/sagernet/sing-box/common/settings"
	"github.com/sagernet/sing-box/experimental/clashapi"
	"github.com/sagernet/sing/common/metadata"
	"nekobox_core/internal/boxbox"
	grpc_server "nekobox_core/server"
	"nekobox_core/server/gen"
	"os"
	"strings"
	"time"

	"github.com/matsuridayo/libneko/neko_common"
	"github.com/matsuridayo/libneko/neko_log"
	"log"
	"nekobox_core/internal/boxapi"
	"nekobox_core/internal/boxmain"

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

	instance, instance_cancel, err = boxmain.Create([]byte(in.CoreConfig))

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

func (s *server) Test(ctx context.Context, in *gen.TestReq) (*gen.TestResp, error) {
	var testInstance *boxbox.Box
	var cancel context.CancelFunc
	var err error
	var twice = true
	if in.TestCurrent {
		if instance == nil {
			return &gen.TestResp{Results: []*gen.URLTestResp{{
				OutboundTag: "proxy",
				LatencyMs:   0,
				Error:       "Instance is not running",
			}}}, nil
		}
		testInstance = instance
		twice = false
	} else {
		testInstance, cancel, err = boxmain.Create([]byte(in.Config))
		if err != nil {
			return nil, err
		}
		defer cancel()
		defer testInstance.Close()
	}

	outboundTags := in.OutboundTags
	if in.UseDefaultOutbound || in.TestCurrent {
		outbound, err := testInstance.Router().DefaultOutbound("tcp")
		if err != nil {
			return nil, err
		}
		outboundTags = []string{outbound.Tag()}
	}

	var maxConcurrency = in.MaxConcurrency
	if maxConcurrency >= 500 || maxConcurrency == 0 {
		maxConcurrency = MaxConcurrentTests
	}
	results := BatchURLTest(testCtx, testInstance, outboundTags, in.Url, int(maxConcurrency), twice)

	res := make([]*gen.URLTestResp, 0)
	for idx, data := range results {
		errStr := ""
		if data.Error != nil {
			errStr = data.Error.Error()
		}
		res = append(res, &gen.URLTestResp{
			OutboundTag: outboundTags[idx],
			LatencyMs:   int32(data.Duration.Milliseconds()),
			Error:       errStr,
		})
	}

	return &gen.TestResp{Results: res}, nil
}

func (s *server) StopTest(ctx context.Context, in *gen.EmptyReq) (*gen.EmptyResp, error) {
	cancelTests()
	testCtx, cancelTests = context.WithCancel(context.Background())

	return &gen.EmptyResp{}, nil
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
	if instance == nil {
		return &gen.ListConnectionsResp{}, nil
	}
	if instance.Router().ClashServer() == nil {
		return nil, errors.New("no clash server found")
	}
	clash, ok := instance.Router().ClashServer().(*clashapi.Server)
	if !ok {
		return nil, errors.New("invalid state, should not be here")
	}
	connections := clash.TrafficManager().Connections()

	res := make([]*gen.ConnectionMetaData, 0)
	for _, c := range connections {
		process := ""
		if c.Metadata.ProcessInfo != nil {
			spl := strings.Split(c.Metadata.ProcessInfo.ProcessPath, string(os.PathSeparator))
			process = spl[len(spl)-1]
		}
		r := &gen.ConnectionMetaData{
			Id:        c.ID.String(),
			CreatedAt: c.CreatedAt.UnixMilli(),
			Upload:    c.Upload.Load(),
			Download:  c.Download.Load(),
			Outbound:  c.Outbound,
			Network:   c.Metadata.Network,
			Dest:      c.Metadata.Destination.String(),
			Protocol:  c.Metadata.Protocol,
			Domain:    c.Metadata.Domain,
			Process:   process,
		}
		res = append(res, r)
	}
	out := &gen.ListConnectionsResp{
		Connections: res,
	}
	return out, nil
}

func (s *server) GetGeoIPList(ctx context.Context, in *gen.GeoListRequest) (*gen.GetGeoIPListResponse, error) {
	resp, err := boxmain.ListGeoip(in.Path + string(os.PathSeparator) + "geoip.db")
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

func (s *server) GetGeoSiteList(ctx context.Context, in *gen.GeoListRequest) (*gen.GetGeoSiteListResponse, error) {
	resp, err := boxmain.GeositeList(in.Path + string(os.PathSeparator) + "geosite.db")
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
	err := boxmain.CompileRuleSet(in.Path+string(os.PathSeparator)+"geoip.db", category, boxmain.IpRuleSet, "./rule_sets/"+in.Item+".srs")
	if err != nil {
		return nil, err
	}

	return &gen.EmptyResp{}, nil
}

func (s *server) CompileGeoSiteToSrs(ctx context.Context, in *gen.CompileGeoSiteToSrsRequest) (*gen.EmptyResp, error) {
	category := strings.TrimSuffix(in.Item, "_SITE")
	err := boxmain.CompileRuleSet(in.Path+string(os.PathSeparator)+"geosite.db", category, boxmain.SiteRuleSet, "./rule_sets/"+in.Item+".srs")
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
