package main

import (
	"Core/internal"
	"Core/internal/boxbox"
	"bytes"
	"context"
	"errors"
	"fmt"
	"github.com/Mahdi-zarei/speedtest-go/speedtest"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing/common/metadata"
	"github.com/sagernet/sing/service"
	"io"
	"net"
	"net/http"
	"sync"
	"time"
)

var testCtx context.Context
var cancelTests context.CancelFunc
var SpTQuerier SpeedTestResultQuerier
var URLReporter URLTestReporter
var CountryResults CountryTestResults

const URLTestTimeout = 3 * time.Second
const FetchServersTimeout = 8 * time.Second
const MaxConcurrentTests = 100

type URLTestResult struct {
	Duration time.Duration
	Tag      string
	Error    error
}

type URLTestReporter struct {
	results []*URLTestResult
	mu      sync.Mutex
}

func (u *URLTestReporter) AddResult(result *URLTestResult) {
	u.mu.Lock()
	defer u.mu.Unlock()
	u.results = append(u.results, result)
}

func (u *URLTestReporter) Results() []*URLTestResult {
	u.mu.Lock()
	defer u.mu.Unlock()
	res := u.results
	u.results = nil
	return res
}

type SpeedTestResult struct {
	Tag           string
	DlSpeed       string
	UlSpeed       string
	Latency       int32
	ServerName    string
	ServerCountry string
	Error         error
	Cancelled     bool
}

type SpeedTestResultQuerier struct {
	isRunning bool
	current   SpeedTestResult
	mu        sync.RWMutex
}

func (s *SpeedTestResultQuerier) Result() (SpeedTestResult, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.current, s.isRunning
}

func (s *SpeedTestResultQuerier) storeResult(result *SpeedTestResult) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.current = *result
}

func (s *SpeedTestResultQuerier) setIsRunning(isRunning bool) {
	s.isRunning = isRunning
}

type CountryTestResults struct {
	results []*SpeedTestResult
	mu      sync.Mutex
}

func (c *CountryTestResults) AddResult(result *SpeedTestResult) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.results = append(c.results, result)
}

func (c *CountryTestResults) Results() []*SpeedTestResult {
	c.mu.Lock()
	defer c.mu.Unlock()
	cp := c.results
	c.results = nil
	return cp
}

func BatchURLTest(ctx context.Context, i *boxbox.Box, outboundTags []string, url string, maxConcurrency int, twice bool, timeout time.Duration) []*URLTestResult {
	if timeout <= 0 {
		timeout = URLTestTimeout
	}
	outbounds := service.FromContext[adapter.OutboundManager](i.Context())
	resMap := make(map[string]*URLTestResult)
	resAccess := sync.Mutex{}
	limiter := make(chan struct{}, maxConcurrency)

	wg := &sync.WaitGroup{}
	wg.Add(len(outboundTags))
	for _, tag := range outboundTags {
		select {
		case <-ctx.Done():
			wg.Done()
			resAccess.Lock()
			resMap[tag] = &URLTestResult{
				Duration: 0,
				Error:    errors.New("test aborted"),
			}
			resAccess.Unlock()
		default:
			time.Sleep(2 * time.Millisecond) // don't spawn goroutines too quickly
			limiter <- struct{}{}
			go func(t string) {
				defer wg.Done()
				outbound, found := outbounds.Outbound(t)
				if !found {
					panic("no outbound with tag " + t + " found")
				}
				client := &http.Client{
					Transport: &http.Transport{
						DialContext: func(ctx context.Context, network string, addr string) (net.Conn, error) {
							return outbound.DialContext(ctx, "tcp", metadata.ParseSocksaddr(addr))
						},
					},
					Timeout: timeout,
				}
				// to properly measure muxed configs, let's do the test twice
				duration, err := urlTest(ctx, client, url)
				if err == nil && twice {
					duration, err = urlTest(ctx, client, url)
				}
				resAccess.Lock()
				u := &URLTestResult{
					Duration: duration,
					Tag:      t,
					Error:    err,
				}
				resMap[t] = u
				URLReporter.AddResult(u)
				resAccess.Unlock()
				<-limiter
			}(tag)
		}
	}

	wg.Wait()
	res := make([]*URLTestResult, 0, len(outboundTags))
	for _, tag := range outboundTags {
		res = append(res, resMap[tag])
	}

	return res
}

func urlTest(ctx context.Context, client *http.Client, url string) (time.Duration, error) {
	begin := time.Now()
	req, err := http.NewRequestWithContext(ctx, "GET", url, nil)
	if err != nil {
		return 0, err
	}
	resp, err := client.Do(req)
	if err != nil {
		return 0, err
	}
	_ = resp.Body.Close()
	return time.Since(begin), nil
}

func getNetDialer(dialer func(ctx context.Context, network string, destination metadata.Socksaddr) (net.Conn, error)) func(ctx context.Context, network string, address string) (net.Conn, error) {
	return func(ctx context.Context, network string, address string) (net.Conn, error) {
		return dialer(ctx, network, metadata.ParseSocksaddr(address))
	}
}

func BatchSpeedTest(ctx context.Context, i *boxbox.Box, outboundTags []string, testDl, testUl bool, simpleDL bool, simpleAddress string, timeout time.Duration, countryOnly bool, countryConcurrency int32) []*SpeedTestResult {
	outbounds := service.FromContext[adapter.OutboundManager](i.Context())
	results := make([]*SpeedTestResult, 0)
	var queuer chan struct{}
	wg := &sync.WaitGroup{}
	if countryOnly {
		if countryConcurrency <= 0 {
			countryConcurrency = 5
		}
		queuer = make(chan struct{}, countryConcurrency)
	}

	for _, tag := range outboundTags {
		select {
		case <-ctx.Done():
			break
		default:
		}
		outbound, exists := outbounds.Outbound(tag)
		if !exists {
			panic("no outbound with tag " + tag + " found")
		}
		res := new(SpeedTestResult)
		res.Tag = tag
		results = append(results, res)

		var err error
		if countryOnly {
			queuer <- struct{}{}
			wg.Add(1)
			go func(res *SpeedTestResult, outbound adapter.Outbound) {
				defer func() { <-queuer }()
				defer wg.Done()
				err := countryTest(ctx, getNetDialer(outbound.DialContext), res)
				if err != nil && !errors.Is(err, context.Canceled) {
					res.Error = err
					fmt.Println("Failed to countryTest with err:", err)
				}
				CountryResults.AddResult(res)
			}(res, outbound)
			continue
		}
		if simpleDL {
			err = simpleDownloadTest(ctx, getNetDialer(outbound.DialContext), res, simpleAddress, timeout)
		} else {
			err = speedTestWithDialer(ctx, getNetDialer(outbound.DialContext), res, testDl, testUl, timeout)
		}
		if err != nil && !errors.Is(err, context.Canceled) {
			res.Error = err
			fmt.Println("Failed to speedtest with err:", err)
		}
		if !testDl && !simpleDL {
			res.DlSpeed = ""
		}
		if !testUl {
			res.UlSpeed = ""
		}
	}
	wg.Wait()

	return results
}

func simpleDownloadTest(ctx context.Context, dialer func(ctx context.Context, network string, address string) (net.Conn, error), res *SpeedTestResult, testURL string, timeout time.Duration) error {
	if timeout <= 0 {
		timeout = URLTestTimeout
	}
	client := &http.Client{
		Transport: &http.Transport{
			DialContext: func(ctx context.Context, network string, addr string) (net.Conn, error) {
				return dialer(ctx, network, addr)
			},
		},
		Timeout: timeout,
	}

	res.ServerName = "N/A"
	res.ServerCountry = "N/A"

	buf := bytes.NewBuffer(make([]byte, 0, 8*(1<<20)))
	req, err := http.NewRequestWithContext(ctx, "GET", testURL, nil)
	if err != nil {
		return err
	}

	done := make(chan struct{})
	var start time.Time
	var latency int32

	go func() {
		defer close(done)
		reqStart := time.Now()
		resp, err := client.Do(req)
		if err != nil {
			res.Error = err
			return
		}
		latency = int32(time.Since(reqStart).Milliseconds())
		start = time.Now()
		_, _ = io.Copy(buf, resp.Body)
	}()

	ticker := time.NewTicker(time.Millisecond * 50)
	defer ticker.Stop()

	SpTQuerier.setIsRunning(true)
	defer SpTQuerier.setIsRunning(false)

	for {
		select {
		case <-done:
			res.DlSpeed = internal.BrateToStr(internal.CalculateBRate(float64(buf.Len()), start))
			res.Latency = latency
			SpTQuerier.storeResult(res)
			return nil
		case <-ctx.Done():
			res.Cancelled = true
			return ctx.Err()
		case <-ticker.C:
			res.DlSpeed = internal.BrateToStr(internal.CalculateBRate(float64(buf.Len()), start))
			res.Latency = latency
			SpTQuerier.storeResult(res)
		}
	}
}

func getSpeedtestServer(ctx context.Context, dialer func(ctx context.Context, network string, address string) (net.Conn, error)) (*speedtest.Server, error) {
	clt := speedtest.New(speedtest.WithUserConfig(&speedtest.UserConfig{
		DialContextFunc: dialer,
		PingMode:        speedtest.HTTP,
		MaxConnections:  8,
	}))
	fetchCtx, cancel := context.WithTimeout(ctx, FetchServersTimeout)
	defer cancel()
	srv, err := clt.FetchServerListContext(fetchCtx)
	if err != nil {
		return nil, err
	}
	srv, err = srv.FindServer(nil)
	if err != nil {
		return nil, err
	}

	if srv.Len() == 0 {
		return nil, errors.New("no server found for speedTest")
	}

	return srv[0], nil
}

func countryTest(ctx context.Context, dialer func(ctx context.Context, network string, address string) (net.Conn, error), res *SpeedTestResult) error {
	srv, err := getSpeedtestServer(ctx, dialer)
	if err != nil {
		return err
	}
	res.ServerName = srv.Name
	res.ServerCountry = srv.Country
	res.Latency = int32(srv.Latency.Milliseconds())
	return nil
}

func speedTestWithDialer(ctx context.Context, dialer func(ctx context.Context, network string, address string) (net.Conn, error), res *SpeedTestResult, testDl, testUl bool, timeout time.Duration) error {
	srv, err := getSpeedtestServer(ctx, dialer)
	if err != nil {
		return err
	}
	res.ServerName = srv.Name
	res.ServerCountry = srv.Country

	done := make(chan struct{})

	SpTQuerier.setIsRunning(true)
	defer SpTQuerier.setIsRunning(false)

	go func() {
		defer func() { close(done) }()
		if testDl {
			timeoutCtx, cancel := context.WithTimeout(ctx, timeout)
			defer cancel()
			err = srv.DownloadTestContext(timeoutCtx)
			if err != nil && !errors.Is(err, context.Canceled) {
				res.Error = err
				return
			}
		}
		if testUl {
			timeoutCtx, cancel := context.WithTimeout(ctx, timeout)
			defer cancel()
			err = srv.UploadTestContext(timeoutCtx)
			if err != nil && !errors.Is(err, context.Canceled) {
				res.Error = err
				return
			}
		}
	}()

	ticker := time.NewTicker(time.Millisecond * 50)
	defer ticker.Stop()

	for {
		select {
		case <-done:
			res.DlSpeed = internal.BrateToStr(float64(srv.DLSpeed))
			res.UlSpeed = internal.BrateToStr(float64(srv.ULSpeed))
			res.Latency = int32(srv.Latency.Milliseconds())
			SpTQuerier.storeResult(res)
			return nil
		case <-ctx.Done():
			res.Cancelled = true
			return ctx.Err()
		case <-ticker.C:
			res.DlSpeed = internal.BrateToStr(srv.Context.GetEWMADownloadRate())
			res.UlSpeed = internal.BrateToStr(srv.Context.GetEWMAUploadRate())
			res.Latency = int32(srv.Latency.Milliseconds())
			SpTQuerier.storeResult(res)
		}
	}
}
