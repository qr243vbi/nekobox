package main

import (
	"Core/gen"
	"Core/internal/boxbox"
	"Core/internal/boxmain"
	"Core/internal/process"
	"Core/internal/sys"
	"context"
	"errors"
	"fmt"
	"log"
	"os"
	"runtime"
	"strings"
	"time"

	"github.com/google/shlex"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/common/settings"
	"github.com/sagernet/sing-box/experimental/clashapi"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/metadata"
	"github.com/sagernet/sing/service"
)

var boxInstance *boxbox.Box
var extraProcess *process.Process
var needUnsetDNS bool
var systemProxyController settings.SystemProxy
var systemProxyAddr metadata.Socksaddr
var instanceCancel context.CancelFunc
var debug bool

// server is used to implement myservice.MyServiceServer.
type server struct {
	gen.UnimplementedLibcoreServiceServer
}

// To returns a pointer to the given value.
func To[T any](v T) *T {
	return &v
}

func (s *server) Start(ctx context.Context, in *gen.LoadConfigReq) (*gen.ErrorResp, error) {
	out := new(gen.ErrorResp)
	var err error

	defer func() {
		if err != nil {
			out.Error = (err.Error())
			boxInstance = nil
		}
	}()

	if debug {
		log.Println("Start:", in.CoreConfig)
	}

	if boxInstance != nil {
		err = errors.New("instance already started")
		return out, nil
	}

	if in.NeedExtraProcess {
		args, e := shlex.Split(in.GetExtraProcessArgs())
		if e != nil {
			err = E.Cause(e, "Failed to parse args")
			return out, nil
		}
		if in.ExtraProcessConf != "" {
			extraConfPath := in.ExtraProcessConfDir + string(os.PathSeparator) + "extra.conf"
			f, e := os.OpenFile(extraConfPath, os.O_CREATE|os.O_TRUNC|os.O_RDWR, 700)
			if e != nil {
				err = E.Cause(e, "Failed to open extra.conf")
				return out, nil
			}
			_, e = f.WriteString(in.ExtraProcessConf)
			if e != nil {
				err = E.Cause(e, "Failed to write extra.conf")
				return out, nil
			}
			_ = f.Close()
			for idx, arg := range args {
				if strings.Contains(arg, "%s") {
					args[idx] = fmt.Sprintf(arg, extraConfPath)
					break
				}
			}
		}

		extraProcess = process.NewProcess(in.ExtraProcessPath, args, in.ExtraNoOut)
		err = extraProcess.Start()
		if err != nil {
			return out, nil
		}
	}

	boxInstance, instanceCancel, err = boxmain.Create([]byte(in.CoreConfig))
	if err != nil {
		return out, nil
	}
	if runtime.GOOS == "darwin" && strings.Contains(
		in.CoreConfig, "tun-in") && strings.Contains(
		in.CoreConfig, "172.19.0.1/24") {
		err := sys.SetSystemDNS("172.19.0.2", boxInstance.Network().InterfaceMonitor())
		if err != nil {
			log.Println("Failed to set system DNS:", err)
		}
		needUnsetDNS = true
	}

	return out, nil
}

func (s *server) Stop(ctx context.Context, in *gen.EmptyReq) (*gen.ErrorResp, error) {
	out := new(gen.ErrorResp)
	var err error

	defer func() {
		if err != nil {
			out.Error = (err.Error())
		}
	}()

	if boxInstance == nil {
		return out, err
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

	if extraProcess != nil {
		extraProcess.Stop()
		extraProcess = nil
	}

	return out, nil
}

func (s *server) CheckConfig(ctx context.Context, in *gen.LoadConfigReq) (*gen.ErrorResp, error) {
	out := new(gen.ErrorResp)
	err := boxmain.Check([]byte(in.CoreConfig))
	if err != nil {
		out.Error = (err.Error())
		return out, nil
	}
	return out, nil
}

func (s *server) Test(ctx context.Context, in *gen.TestReq) (*gen.TestResp, error) {
	out := new(gen.TestResp)
	var testInstance *boxbox.Box
	var cancel context.CancelFunc
	var err error
	var twice = true
	if in.TestCurrent {
		if boxInstance == nil {
			out.Results = []*gen.URLTestResp{{
				OutboundTag: ("proxy"),
				LatencyMs:   (int32(0)),
				Error:       ("Instance is not running"),
			}}
			return out, nil
		}
		testInstance = boxInstance
		twice = false
	} else {
		testInstance, cancel, err = boxmain.Create([]byte(in.Config))
		if err != nil {
			return out, err
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
	results := BatchURLTest(testCtx, testInstance, outboundTags, in.Url,
		int(maxConcurrency), twice, time.Duration(in.TestTimeoutMs)*time.Millisecond)

	res := make([]*gen.URLTestResp, 0)
	for idx, data := range results {
		errStr := ""
		if data.Error != nil {
			errStr = data.Error.Error()
		}
		res = append(res, &gen.URLTestResp{
			OutboundTag: (outboundTags[idx]),
			LatencyMs:   (int32(data.Duration.Milliseconds())),
			Error:       (errStr),
		})
	}

	out.Results = res
	return out, nil
}

func (s *server) StopTest(ctx context.Context, in *gen.EmptyReq) (*gen.EmptyResp, error) {
	out := new(gen.EmptyResp)

	cancelTests()
	testCtx, cancelTests = context.WithCancel(context.Background())

	return out, nil
}

func (s *server) QueryURLTest(ctx context.Context, in *gen.EmptyReq) (*gen.QueryURLTestResponse, error) {
	out := new(gen.QueryURLTestResponse)
	results := URLReporter.Results()
	for _, r := range results {
		errStr := ""
		if r.Error != nil {
			errStr = r.Error.Error()
		}
		out.Results = append(out.Results, &gen.URLTestResp{
			OutboundTag: (r.Tag),
			LatencyMs:   (int32(r.Duration.Milliseconds())),
			Error:       (errStr),
		})
	}
	return out, nil
}

func (s *server) QueryStats(ctx context.Context, in *gen.EmptyReq) (*gen.QueryStatsResp, error) {
	out := new(gen.QueryStatsResp)
	out.Ups = make(map[string]int64)
	out.Downs = make(map[string]int64)
	if boxInstance != nil {
		clash := service.FromContext[adapter.ClashServer](boxInstance.Context())
		if clash != nil {
			cApi, ok := clash.(*clashapi.Server)
			if !ok {
				log.Println("Failed to assert clash server")
				return out, E.New("invalid clash server type")
			}
			outbounds := service.FromContext[adapter.OutboundManager](boxInstance.Context())
			if outbounds == nil {
				log.Println("Failed to get outbound manager")
				return out, E.New("nil outbound manager")
			}
			endpoints := service.FromContext[adapter.EndpointManager](boxInstance.Context())
			if endpoints == nil {
				log.Println("Failed to get endpoint manager")
				return out, E.New("nil endpoint manager")
			}
			for _, ob := range outbounds.Outbounds() {
				u, d := cApi.TrafficManager().TotalOutbound(ob.Tag())
				out.Ups[ob.Tag()] = u
				out.Downs[ob.Tag()] = d
			}
			for _, ep := range endpoints.Endpoints() {
				u, d := cApi.TrafficManager().TotalOutbound(ep.Tag())
				out.Ups[ep.Tag()] = u
				out.Downs[ep.Tag()] = d
			}
		}
	}
	return out, nil
}

func (s *server) ListConnections(ctx context.Context, in *gen.EmptyReq) (*gen.ListConnectionsResp, error) {
	out := new(gen.ListConnectionsResp)
	if boxInstance == nil {
		return out, nil
	}
	if service.FromContext[adapter.ClashServer](boxInstance.Context()) == nil {
		return out, errors.New("no clash server found")
	}
	clash, ok := service.FromContext[adapter.ClashServer](boxInstance.Context()).(*clashapi.Server)
	if !ok {
		return out, errors.New("invalid state, should not be here")
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
			Id:        (c.ID.String()),
			CreatedAt: (c.CreatedAt.UnixMilli()),
			Upload:    (c.Upload.Load()),
			Download:  (c.Download.Load()),
			Outbound:  (c.Outbound),
			Network:   (c.Metadata.Network),
			Dest:      (c.Metadata.Destination.String()),
			Protocol:  (c.Metadata.Protocol),
			Domain:    (c.Metadata.Domain),
			Process:   (process),
		}
		res = append(res, r)
	}
	out.Connections = res
	return out, nil
}

func (s *server) SpeedTest(ctx context.Context, in *gen.SpeedTestRequest) (*gen.SpeedTestResponse, error) {
	out := new(gen.SpeedTestResponse)
	if !in.TestDownload && !in.TestUpload && !in.SimpleDownload && !in.OnlyCountry {
		return out, errors.New("cannot run empty test")
	}
	var testInstance *boxbox.Box
	var cancel context.CancelFunc
	outboundTags := in.OutboundTags
	var err error
	if in.TestCurrent {
		if boxInstance == nil {
			out.Results = []*gen.SpeedTestResult{{
				OutboundTag: ("proxy"),
				Error:       ("Instance is not running"),
			}}
			return out, nil
		}
		testInstance = boxInstance
	} else {
		testInstance, cancel, err = boxmain.Create([]byte(in.Config))
		if err != nil {
			return out, err
		}
		defer cancel()
		defer testInstance.Close()
	}

	if in.UseDefaultOutbound || in.TestCurrent {
		outbound := testInstance.Outbound().Default()
		outboundTags = []string{outbound.Tag()}
	}

	results := BatchSpeedTest(testCtx, testInstance, outboundTags,
		in.TestDownload, in.TestUpload, in.SimpleDownload,
		in.SimpleDownloadAddr, time.Duration(in.TimeoutMs)*time.Millisecond,
		in.OnlyCountry, in.CountryConcurrency)

	res := make([]*gen.SpeedTestResult, 0)
	for _, data := range results {
		errStr := ""
		if data.Error != nil {
			errStr = data.Error.Error()
		}
		res = append(res, &gen.SpeedTestResult{
			DlSpeed:       (data.DlSpeed),
			UlSpeed:       (data.UlSpeed),
			Latency:       (data.Latency),
			OutboundTag:   (data.Tag),
			Error:         (errStr),
			ServerName:    (data.ServerName),
			ServerCountry: (data.ServerCountry),
			Cancelled:     (data.Cancelled),
		})
	}

	out.Results = res
	return out, nil
}

func (s *server) QuerySpeedTest(ctx context.Context, in *gen.EmptyReq) (*gen.QuerySpeedTestResponse, error) {
	out := new(gen.QuerySpeedTestResponse)
	res, isRunning := SpTQuerier.Result()
	errStr := ""
	if res.Error != nil {
		errStr = res.Error.Error()
	}
	out.Result = &gen.SpeedTestResult{
		DlSpeed:       (res.DlSpeed),
		UlSpeed:       (res.UlSpeed),
		Latency:       (res.Latency),
		OutboundTag:   (res.Tag),
		Error:         (errStr),
		ServerName:    (res.ServerName),
		ServerCountry: (res.ServerCountry),
		Cancelled:     (res.Cancelled),
	}
	out.IsRunning = (isRunning)
	return out, nil
}

func (s *server) QueryCountryTest(ctx context.Context, in *gen.EmptyReq) (*gen.QueryCountryTestResponse, error) {
	out := new(gen.QueryCountryTestResponse)
	results := CountryResults.Results()
	for _, res := range results {
		var errStr string
		if res.Error != nil {
			errStr = res.Error.Error()
		}
		out.Results = append(out.Results, &gen.SpeedTestResult{
			DlSpeed:       (res.DlSpeed),
			UlSpeed:       (res.UlSpeed),
			Latency:       (res.Latency),
			OutboundTag:   (res.Tag),
			Error:         (errStr),
			ServerName:    (res.ServerName),
			ServerCountry: (res.ServerCountry),
			Cancelled:     (res.Cancelled),
		})
	}
	return out, nil
}
