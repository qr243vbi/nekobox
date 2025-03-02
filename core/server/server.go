package main

import (
	"context"
	"encoding/json"
	"errors"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/common/settings"
	"github.com/sagernet/sing-box/experimental/clashapi"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/metadata"
	"github.com/sagernet/sing/service"
	"io"
	"log"
	"nekobox_core/gen"
	"nekobox_core/internal/boxapi"
	"nekobox_core/internal/boxbox"
	"nekobox_core/internal/boxdns"
	"nekobox_core/internal/boxmain"
	"nekobox_core/internal/sys"
	"net/http"
	"os"
	"runtime"
	"strings"
	"time"
)

var boxInstance *boxbox.Box
var needUnsetDNS bool
var systemProxyController settings.SystemProxy
var systemProxyAddr metadata.Socksaddr
var instanceCancel context.CancelFunc
var debug bool

type server struct {
	gen.UnimplementedLibcoreServiceServer
}

func (s *server) Exit(ctx context.Context, in *gen.EmptyReq) (out *gen.EmptyResp, _ error) {
	out = &gen.EmptyResp{}

	// Connection closed
	os.Exit(0)
	return
}

func (s *server) Start(ctx context.Context, in *gen.LoadConfigReq) (out *gen.ErrorResp, _ error) {
	var err error

	defer func() {
		out = &gen.ErrorResp{}
		if err != nil {
			out.Error = err.Error()
			boxInstance = nil
		}
	}()

	if debug {
		log.Println("Start:", in.CoreConfig)
	}

	if boxInstance != nil {
		err = errors.New("instance already started")
		return
	}

	boxInstance, instanceCancel, err = boxmain.Create([]byte(in.CoreConfig))
	if err != nil {
		return
	}
	if runtime.GOOS == "windows" {
		nm := boxInstance.Network()
		boxdns.DefaultIfcMonitor = nm.InterfaceMonitor()
		boxdns.HandleInterfaceChange(nil, 0)
		boxdns.DefaultIfcMonitor.RegisterCallback(boxdns.HandleInterfaceChange)
	}
	if runtime.GOOS == "darwin" && strings.Contains(in.CoreConfig, "utun") {
		err := sys.SetSystemDNS("172.19.0.2", boxInstance.Network().InterfaceMonitor())
		if err != nil {
			log.Println("Failed to set system DNS:", err)
		}
		needUnsetDNS = true
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

	if boxInstance == nil {
		return
	}

	if needUnsetDNS {
		needUnsetDNS = false
		err := sys.SetSystemDNS("Empty", boxInstance.Network().InterfaceMonitor())
		if err != nil {
			log.Println("Failed to unset system DNS:", err)
		}
	}
	boxInstance.CloseWithTimeout(instanceCancel, time.Second*2, log.Println)

	boxInstance = nil

	return
}

func (s *server) CheckConfig(ctx context.Context, in *gen.LoadConfigReq) (*gen.ErrorResp, error) {
	err := boxmain.Check([]byte(in.CoreConfig))
	if err != nil {
		return &gen.ErrorResp{
			Error: err.Error(),
		}, nil
	}
	return &gen.ErrorResp{}, nil
}

func (s *server) Test(ctx context.Context, in *gen.TestReq) (*gen.TestResp, error) {
	var testInstance *boxbox.Box
	var cancel context.CancelFunc
	var err error
	var twice = true
	if in.TestCurrent {
		if boxInstance == nil {
			return &gen.TestResp{Results: []*gen.URLTestResp{{
				OutboundTag: "proxy",
				LatencyMs:   0,
				Error:       "Instance is not running",
			}}}, nil
		}
		testInstance = boxInstance
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
		outbound := testInstance.Outbound().Default()
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

func (s *server) QueryStats(ctx context.Context, _ *gen.EmptyReq) (*gen.QueryStatsResp, error) {
	resp := &gen.QueryStatsResp{
		Ups:   make(map[string]int64),
		Downs: make(map[string]int64),
	}
	if boxInstance != nil {
		clash := service.FromContext[adapter.ClashServer](boxInstance.Context())
		if clash != nil {
			cApi, ok := clash.(*clashapi.Server)
			if !ok {
				log.Println("Failed to assert clash server")
				return nil, E.New("invalid clash server type")
			}
			outbounds := service.FromContext[adapter.OutboundManager](boxInstance.Context())
			endpoints := service.FromContext[adapter.EndpointManager](boxInstance.Context())
			if outbounds == nil {
				log.Println("Failed to assert outbound manager")
				return nil, E.New("invalid outbound manager type")
			}
			if endpoints == nil {
				log.Println("Failed to assert endpoint manager")
				return nil, E.New("invalid endpoint manager type")
			}
			for _, out := range outbounds.Outbounds() {
				if len(out.Dependencies()) > 0 {
					// ignore, has detour
					continue
				}
				u, d := cApi.TrafficManager().TotalOutbound(out.Tag())
				resp.Ups[out.Tag()] = u
				resp.Downs[out.Tag()] = d
			}
			for _, end := range endpoints.Endpoints() {
				if len(end.Dependencies()) > 0 {
					// ignore, has detour
					continue
				}
				u, d := cApi.TrafficManager().TotalOutbound(end.Tag())
				resp.Ups[end.Tag()] = u
				resp.Downs[end.Tag()] = d
			}
		}
	}

	return resp, nil
}

func (s *server) ListConnections(ctx context.Context, in *gen.EmptyReq) (*gen.ListConnectionsResp, error) {
	if boxInstance == nil {
		return &gen.ListConnectionsResp{}, nil
	}
	if service.FromContext[adapter.ClashServer](boxInstance.Context()) == nil {
		return nil, errors.New("no clash server found")
	}
	clash, ok := service.FromContext[adapter.ClashServer](boxInstance.Context()).(*clashapi.Server)
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

var updateDownloadUrl string

func (s *server) Update(ctx context.Context, in *gen.UpdateReq) (*gen.UpdateResp, error) {
	ret := &gen.UpdateResp{}

	client := boxapi.CreateProxyHttpClient(boxInstance)

	if in.Action == gen.UpdateAction_Check { // Check update
		ctx, cancel := context.WithTimeout(ctx, time.Second*10)
		defer cancel()

		req, _ := http.NewRequestWithContext(ctx, "GET", "https://api.github.com/repos/Mahdi-zarei/nekoray/releases", nil)
		resp, err := client.Do(req)
		if err != nil {
			ret.Error = err.Error()
			return ret, nil
		}
		defer resp.Body.Close()

		var v []struct {
			HtmlUrl string `json:"html_url"`
			Assets  []struct {
				Name               string `json:"name"`
				BrowserDownloadUrl string `json:"browser_download_url"`
			} `json:"assets"`
			Prerelease bool   `json:"prerelease"`
			Body       string `json:"body"`
		}
		err = json.NewDecoder(resp.Body).Decode(&v)
		if err != nil {
			ret.Error = err.Error()
			return ret, nil
		}

		var search string
		if runtime.GOOS == "windows" && runtime.GOARCH == "amd64" {
			search = "windows64"
		} else if runtime.GOOS == "linux" && runtime.GOARCH == "amd64" {
			search = "linux64"
		} else if runtime.GOOS == "darwin" {
			search = "macos-" + runtime.GOARCH
		} else {
			ret.Error = "Not official support platform"
			return ret, nil
		}

		for _, release := range v {
			if len(release.Assets) > 0 {
				for _, asset := range release.Assets {
					if strings.Contains(asset.Name, search) {
						updateDownloadUrl = asset.BrowserDownloadUrl
						ret.AssetsName = asset.Name
						ret.DownloadUrl = asset.BrowserDownloadUrl
						ret.ReleaseUrl = release.HtmlUrl
						ret.ReleaseNote = release.Body
						ret.IsPreRelease = release.Prerelease
						return ret, nil // update
					}
				}
			}
		}
	} else { // Download update
		if updateDownloadUrl == "" {
			ret.Error = "?"
			return ret, nil
		}

		req, _ := http.NewRequestWithContext(ctx, "GET", updateDownloadUrl, nil)
		resp, err := client.Do(req)
		if err != nil {
			ret.Error = err.Error()
			return ret, nil
		}
		defer resp.Body.Close()

		f, err := os.OpenFile("../nekoray.zip", os.O_TRUNC|os.O_CREATE|os.O_RDWR, 0644)
		if err != nil {
			ret.Error = err.Error()
			return ret, nil
		}
		defer f.Close()

		_, err = io.Copy(f, resp.Body)
		if err != nil {
			ret.Error = err.Error()
			return ret, nil
		}
		f.Sync()
	}

	return ret, nil
}
