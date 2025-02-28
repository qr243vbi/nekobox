package main

import (
	"context"
	"errors"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing/common/metadata"
	"github.com/sagernet/sing/service"
	"nekobox_core/internal/boxbox"
	"net"
	"net/http"
	"sync"
	"time"
)

var testCtx context.Context
var cancelTests context.CancelFunc

const URLTestTimeout = 3 * time.Second
const MaxConcurrentTests = 100

type URLTestResult struct {
	Duration time.Duration
	Error    error
}

func BatchURLTest(ctx context.Context, i *boxbox.Box, outboundTags []string, url string, maxConcurrency int, twice bool) []*URLTestResult {
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
						DialContext: func(_ context.Context, network string, addr string) (net.Conn, error) {
							return outbound.DialContext(ctx, "tcp", metadata.ParseSocksaddr(addr))
						},
					},
					Timeout: URLTestTimeout,
				}
				// to properly measure muxed configs, let's do the test twice
				duration, err := urlTest(ctx, client, url)
				if err == nil && twice {
					duration, err = urlTest(ctx, client, url)
				}
				resAccess.Lock()
				resMap[t] = &URLTestResult{
					Duration: duration,
					Error:    err,
				}
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
	ctx, cancel := context.WithTimeout(ctx, URLTestTimeout)
	defer cancel()
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
