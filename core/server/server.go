//go:build !darwin

package main

import (
	"context"
	"errors"
	"fmt"
	"log"
	"nekobox_core/gen"
	"nekobox_core/internal"
	"nekobox_core/internal/boxbox"
	"nekobox_core/internal/boxmain"
	"nekobox_core/internal/process"
	"nekobox_core/internal/wg"

	//	"nekobox_core/internal/sys"
	"net"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/google/shlex"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/common/trafficcontrol"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/service"
)

// server is used to implement myservice.MyServiceServer.
type server struct {
}

// To returns a pointer to the given value.
func To[T any](v T) *T {
	return &v
}

func fileExists(path string) bool {
	info, err := os.Stat(path)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return false
		}
		return false
	}
	return !info.IsDir()
}

func (s *server) IsSupported(ctx context.Context, in *gen.Type) (*gen.Supported, error) {
	out := new(gen.Supported)
	ok := true
	if in.Type == "naive" {
		ok = CheckNaive()
	}
	out.Ok = ok
	return out, nil
}

func (s *server) CacheHTTP(ctx context.Context, in *gen.CacheURLRequest) (*gen.CacheURLResult_, error) {
	out := new(gen.CacheURLResult_)
	if in.Clear && in.GetRefresh() {
		return nil, errors.New("cannot clear and refresh ruleset cache simultaneously")
	}
	if in.Clear {
		if err := internal.ClearRulesets(); err != nil {
			return nil, err
		}
		return out, nil
	}
	if in.GetRefresh() {
		out.FilePath = internal.RefreshHttp(in.HTTPURL, in.UseDefaultOutbound)
		out.Exists = out.FilePath != ""
		return out, nil
	}
	var exists bool
	out.FilePath = internal.CacheHttpBool(in.HTTPURL, in.UseDefaultOutbound, &exists)
	out.Exists = exists
	return out, nil
}

func (s *server) Start(ctx context.Context, in *gen.LoadConfigReq) (*gen.ErrorResp, error) {
	out := new(gen.ErrorResp)
	var err error

	defer func() {
		if err != nil {
			out.Error = err.Error()
			internal.BoxInstance = nil
		} else if internal.BoxInstance != nil && internal.BoxInstance.StartCtx != nil {
			context.AfterFunc(internal.BoxInstance.StartCtx, func() {
				_ = internal.ResetSystemProxy()
			})
		}
	}()

	if internal.Debug {
		log.Println("Start:", in.CoreConfig)
	}

	if internal.BoxInstance != nil {
		err = errors.New("instance already started")
		return out, nil
	}

	if in.NeedExtraProcess {
		args, e := shlex.Split(in.GetExtraProcessArgs_())
		if e != nil {
			err = E.Cause(e, "Failed to parse args")
			return out, nil
		}
		if in.ExtraProcessConf != "" {
			extraConfPath := in.ExtraProcessConfDir + string(os.PathSeparator) + "extra.conf"
			f, e := os.OpenFile(extraConfPath, os.O_CREATE|os.O_TRUNC|os.O_RDWR, 0700)
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

		internal.ExtraProcess = process.NewProcess(in.ExtraProcessPath, args, in.ExtraNoOut)
		err = internal.ExtraProcess.Start()
		if err != nil {
			return out, nil
		}
		if addrs := process.LocalInboundAddrs(in.ExtraProcessConf); len(addrs) > 0 {
			if e := internal.ExtraProcess.WaitListening(addrs, 10*time.Second); e != nil {
				log.Println(e)
			}
		}
	}

	internal.BoxInstance, internal.InstanceCancel, err = boxmain.Create([]byte(in.CoreConfig))
	if err != nil {
		return out, nil
	}

	return out, nil
}

func (s *server) EnableSystemProxy(ctx context.Context, in *gen.SystemProxy) (*gen.ErrorResp, error) {
	out := new(gen.ErrorResp)
	var err error

	defer func() {
		if err != nil {
			out.Error = (err.Error())
		}
	}()

	err = internal.SetSystemProxy(ctx, in.Address, uint16(in.Port), in.SupportSocks)

	return out, nil
}

func (s *server) DisableSystemProxy(ctx context.Context, in *gen.EmptyReq) (*gen.ErrorResp, error) {
	out := new(gen.ErrorResp)
	var err error

	defer func() {
		if err != nil {
			out.Error = (err.Error())
		}
	}()

	err = internal.ResetSystemProxy()

	return out, nil
}

func (s *server) Stop(ctx context.Context, in *gen.EmptyReq) (*gen.ErrorResp, error) {
	out := new(gen.ErrorResp)
	var err error

	_ = internal.ResetSystemProxy()

	defer func() {
		if err != nil {
			out.Error = err.Error()
		}
	}()

	if internal.BoxInstance == nil {
		return out, err
	}

	if internal.InstanceCancel != nil {
		internal.BoxInstance.CloseWithTimeout(internal.InstanceCancel, 2*time.Second, log.Println)
	} else {
		_ = internal.BoxInstance.Close()
	}

	internal.BoxInstance = nil
	internal.InstanceCancel = nil

	if internal.ExtraProcess != nil {
		internal.ExtraProcess.Stop()
		internal.ExtraProcess = nil
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

func (s *server) QueryIPTest(ctx context.Context, in *gen.EmptyReq) (out *gen.QueryIPTestResponse, _ error) {
	results := IPReporter.Results()
	out = &gen.QueryIPTestResponse{}
	for _, r := range results {
		errStr := ""
		if r.Error != nil {
			errStr = r.Error.Error()
		}

		out.Results = append(out.Results, &gen.IPTestResp{
			OutboundTag: (r.Tag),
			IP:          (r.Result.IP),
			CountryCode: (r.Result.CountryCode),
			Error:       (errStr),
		})
	}
	return out, nil
}

func (s *server) IPTest(ctx context.Context, in *gen.IPTestRequest) (*gen.QueryIPTestResponse, error) {
	out := new(gen.QueryIPTestResponse)
	var testInstance *boxbox.Box
	var cancel context.CancelFunc
	var err error

	testInstance, cancel, err = boxmain.Create([]byte(in.Config))
	if err != nil {
		return nil, err
	}
	if cancel != nil {
		defer cancel()
	}
	defer func() {
		if testInstance != nil {
			_ = testInstance.Close()
		}
	}()

	outboundTags := in.OutboundTags
	if in.UseDefaultOutbound {
		outbound := testInstance.Outbound().Default()
		outboundTags = []string{outbound.Tag()}
	}

	maxConcurrency := in.MaxConcurrency
	if maxConcurrency >= MaxConcurrentTests || maxConcurrency == 0 {
		maxConcurrency = MaxConcurrentTests
	}
	timeout := time.Duration(in.TestTimeoutMs) * time.Millisecond
	results := BatchIPTest(testCtx, testInstance, outboundTags, int(maxConcurrency), timeout)

	res := make([]*gen.IPTestResp, 0, len(results))
	for idx, data := range results {
		errStr := ""
		if data.Error != nil {
			errStr = data.Error.Error()
		}
		tag := outboundTags[idx]

		res = append(res, &gen.IPTestResp{
			OutboundTag: (tag),
			IP:          (data.Result.IP),
			CountryCode: (data.Result.CountryCode),
			Error:       (errStr),
		})
	}
	out.Results = res
	return out, nil
}

// prefer the requested tags, the default one follows route.final
func resolveLiveTags(i *boxbox.Box, tags []string) []string {
	outbounds := service.FromContext[adapter.OutboundManager](i.Context())
	known := make([]string, 0, len(tags))
	for _, tag := range tags {
		if _, found := outbounds.Outbound(tag); found {
			known = append(known, tag)
		}
	}
	if len(known) == 0 {
		return []string{i.Outbound().Default().Tag()}
	}
	return known
}

func (s *server) Test(ctx context.Context, in *gen.TestReq) (*gen.TestResp, error) {
	out := new(gen.TestResp)
	var testInstance *boxbox.Box
	var cancel context.CancelFunc
	var err error
	var twice = true

	if in.TestCurrent {
		if internal.BoxInstance == nil {
			out.Results = []*gen.URLTestResp{{
				OutboundTag: "proxy",
				LatencyMs:   0,
				Error:       "Instance is not running",
			}}
			return out, nil
		}
		testInstance = internal.BoxInstance
	} else {
		testInstance, cancel, err = boxmain.Create([]byte(in.Config))
		if err != nil {
			return out, err
		}
		if cancel != nil {
			defer cancel()
		}
		defer func() {
			if testInstance != nil {
				_ = testInstance.Close()
			}
		}()
	}

	outboundTags := in.OutboundTags
	if in.TestCurrent {
		outboundTags = resolveLiveTags(testInstance, in.OutboundTags)
	} else if in.UseDefaultOutbound {
		outbound := testInstance.Outbound().Default()
		outboundTags = []string{outbound.Tag()}
	}

	var maxConcurrency = in.MaxConcurrency
	if maxConcurrency >= 500 || maxConcurrency == 0 {
		maxConcurrency = MaxConcurrentTests
	}
	results := BatchURLTest(testCtx, testInstance, outboundTags, in.URL,
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

	box := internal.BoxInstance
	if box == nil {
		return out, nil
	}
	manager := service.PtrFromContext[trafficcontrol.Manager](box.Context())
	if manager == nil {
		return out, nil
	}
	statsCounters.collect(box, manager, out.Ups, out.Downs)
	return out, nil
}

// trafficCounters turns the tracker's cumulative per-connection byte counts
// into what the GUI expects from QueryStats: bytes per outbound tag since the
// previous call.
type trafficCounters struct {
	mu   sync.Mutex
	box  *boxbox.Box
	seen map[string][2]int64 // connection ID -> upload, download already reported
}

var statsCounters trafficCounters

func (c *trafficCounters) collect(box *boxbox.Box, manager *trafficcontrol.Manager, ups, downs map[string]int64) {
	c.mu.Lock()
	defer c.mu.Unlock()
	// Open connections first: one closing meanwhile is then in the closed list.
	open := manager.Connections()
	c.collectFrom(box, open, manager.ClosedConnections(), ups, downs)
}

// collectFrom does the work of collect on listings already taken; the caller
// holds c.mu.
func (c *trafficCounters) collectFrom(box *boxbox.Box, open, closed []*trafficcontrol.TrackerMetadata, ups, downs map[string]int64) {
	if c.box != box {
		c.box = box
		c.seen = make(map[string][2]int64)
	}
	next := make(map[string][2]int64, len(c.seen))
	add := func(connections []*trafficcontrol.TrackerMetadata) {
		for _, metadata := range connections {
			id := metadata.ID.String()
			current := [2]int64{metadata.Upload.Load(), metadata.Download.Load()}
			// A connection that closes between the two listings shows up in both;
			// measuring against what this call already counted avoids doubling it.
			previous, ok := next[id]
			if !ok {
				previous = c.seen[id]
			}
			next[id] = current
			up, down := current[0]-previous[0], current[1]-previous[1]
			if up == 0 && down == 0 {
				continue
			}
			for _, tag := range trackerTags(metadata) {
				ups[tag] += up
				downs[tag] += down
			}
		}
	}
	add(open)
	add(closed)
	c.seen = next
}

// trackerTags returns the outbound groups a connection went through and the
// outbound that carried it, each once.
func trackerTags(metadata *trafficcontrol.TrackerMetadata) []string {
	tags := make([]string, 0, len(metadata.Chain)+1)
	for _, tag := range append(metadata.Chain, metadata.Outbound) {
		if tag != "" && !containsString(tags, tag) {
			tags = append(tags, tag)
		}
	}
	return tags
}

func containsString(values []string, value string) bool {
	for _, item := range values {
		if item == value {
			return true
		}
	}
	return false
}

func destHostPort(dest string) (string, string) {
	if strings.HasPrefix(dest, "[") {
		end := strings.Index(dest, "]:")
		if end > 1 {
			return dest[1:end], dest[end+2:]
		}
		end = strings.Index(dest, "]")
		if end > 1 {
			return dest[1:end], ""
		}
		return dest, ""
	}
	if host, port, err := net.SplitHostPort(dest); err == nil {
		return host, port
	}
	return dest, ""
}

func isUninformativeConnection(process, dest, protocol, outbound string) bool {
	if isNekoboxCoreConnection(process, outbound) {
		return true
	}
	host, port := destHostPort(dest)
	if host == "172.19.0.2" || host == "fdfe:dcba:9876::2" {
		return true
	}
	dnsPort := port == "53" || port == "853"
	if dnsPort && (host == "172.19.0.1" || host == "fdfe:dcba:9876::1") {
		return true
	}
	if process != "" {
		return false
	}
	if strings.EqualFold(protocol, "dns") {
		return true
	}
	return dnsPort
}

func isNekoboxCoreConnection(process, outbound string) bool {
	normalize := func(raw string) string {
		base := strings.ToLower(filepath.Base(raw))
		base = strings.TrimSuffix(base, ".exe")
		base = strings.ReplaceAll(base, "-", "_")
		return base
	}
	p := normalize(process)
	o := normalize(outbound)
	return p == "nekobox_core" || p == "nekoboxcore" ||
		o == "nekobox_core" || o == "nekoboxcore"
}

func (s *server) ListConnections(ctx context.Context, in *gen.EmptyReq) (*gen.ListConnectionsResp, error) {
	out := new(gen.ListConnectionsResp)
	if internal.BoxInstance == nil {
		return out, nil
	}

	manager := service.PtrFromContext[trafficcontrol.Manager](internal.BoxInstance.Context())
	if manager == nil {
		return out, errors.New("no traffic manager found")
	}

	res := make([]*gen.ConnectionMetaData, 0)
	for _, c := range manager.Connections() {
		process := ""
		if c.Metadata.ProcessInfo != nil {
			// filepath.Base handles both / and \ so a Qt-style path still
			// yields just the executable name on Windows.
			process = filepath.Base(c.Metadata.ProcessInfo.ProcessPath)
		}
		if isUninformativeConnection(process, c.Metadata.Destination.String(),
			c.Metadata.Protocol, c.Outbound) {
			continue
		}
		res = append(res, &gen.ConnectionMetaData{
			ID:        c.ID.String(),
			CreatedAt: c.CreatedAt.UnixMilli(),
			Upload:    c.Upload.Load(),
			Download:  c.Download.Load(),
			Outbound:  c.Outbound,
			Network:   c.Metadata.Network,
			Dest:      c.Metadata.Destination.String(),
			Protocol:  c.Metadata.Protocol,
			Domain:    c.Metadata.Domain,
			Process:   process,
		})
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
		if internal.BoxInstance == nil {
			out.Results = []*gen.SpeedTestResult_{{
				OutboundTag: "proxy",
				Error:       "Instance is not running",
			}}
			return out, nil
		}
		testInstance = internal.BoxInstance
	} else {
		testInstance, cancel, err = boxmain.Create([]byte(in.Config))
		if err != nil {
			return out, err
		}
		if cancel != nil {
			defer cancel()
		}
		defer func() {
			if testInstance != nil {
				_ = testInstance.Close()
			}
		}()
	}

	if in.TestCurrent {
		outboundTags = resolveLiveTags(testInstance, in.OutboundTags)
	} else if in.UseDefaultOutbound {
		outbound := testInstance.Outbound().Default()
		outboundTags = []string{outbound.Tag()}
	}

	results := BatchSpeedTest(testCtx, testInstance, outboundTags,
		in.TestDownload, in.TestUpload, in.SimpleDownload,
		in.SimpleDownloadAddr, time.Duration(in.TimeoutMs)*time.Millisecond,
		in.OnlyCountry, in.CountryConcurrency)

	res := make([]*gen.SpeedTestResult_, 0)
	for _, data := range results {
		errStr := ""
		if data.Error != nil {
			errStr = data.Error.Error()
		}
		res = append(res, &gen.SpeedTestResult_{
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
	out.Result_ = &gen.SpeedTestResult_{
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

func (s *server) GenWgKeyPair(ctx context.Context, _ *gen.EmptyReq) (out *gen.GenWgKeyPairResponse, _ error) {
	res := new(gen.GenWgKeyPairResponse)
	privateKey, err := wg.GeneratePrivateKey()
	if err != nil {
		res.Error = err.Error()
		return res, nil
	}
	res.PrivateKey = privateKey.String()
	res.PublicKey = privateKey.PublicKey().String()
	return res, nil
}

func (s *server) QueryCountryTest(ctx context.Context, in *gen.EmptyReq) (*gen.QueryCountryTestResponse, error) {
	out := new(gen.QueryCountryTestResponse)
	results := CountryResults.Results()
	for _, res := range results {
		var errStr string
		if res.Error != nil {
			errStr = res.Error.Error()
		}
		out.Results = append(out.Results, &gen.SpeedTestResult_{
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
