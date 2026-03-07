package speedtest

import (
	"nekobox_core/speedtest/transport"
	"context"
	"errors"
	"fmt"
	"io"
	"math"
	"net/http"
	"net/url"
	"path"
	"sync/atomic"
	"time"
)

type (
	downloadFunc func(context.Context, *Server, int) error
	uploadFunc   func(context.Context, *Server, int) error
)

var (
	dlSizes = [...]int{350, 500, 750, 1000, 1500, 2000, 2500, 3000, 3500, 4000}
	ulSizes = [...]int{100, 300, 500, 800, 1000, 1500, 2500, 3000, 3500, 4000} // kB
)

var (
	ErrConnectTimeout = errors.New("server connect timeout")
)

func (s *Server) MultiDownloadTestContext(ctx context.Context, servers Servers) error {
	ss := servers.Available()
	if ss.Len() == 0 {
		return errors.New("not found available servers")
	}
	mainIDIndex := 0
	var td *TestDirection
	_context, cancel := context.WithCancel(ctx)
	defer cancel()
	var errorTimes int64 = 0
	var requestTimes int64 = 0
	for i, server := range *ss {
		if server.ID == s.ID {
			mainIDIndex = i
		}
		sp := server
		dbg.Printf("Register Download Handler: %s\n", sp.URL)
		td = server.Context.RegisterDownloadHandler(func() {
			atomic.AddInt64(&requestTimes, 1)
			if err := downloadRequest(_context, sp, 3); err != nil {
				atomic.AddInt64(&errorTimes, 1)
			}
		})
	}
	if td == nil {
		return ErrorUninitializedManager
	}
	td.Start(ctx, cancel, mainIDIndex) // block here
	s.DLSpeed = ByteRate(td.manager.GetEWMADownloadRate())
	if s.DLSpeed == 0 && float64(errorTimes)/float64(requestTimes) > 0.1 {
		s.DLSpeed = -1 // N/A
	}
	return nil
}

func (s *Server) MultiUploadTestContext(ctx context.Context, servers Servers) error {
	ss := servers.Available()
	if ss.Len() == 0 {
		return errors.New("not found available servers")
	}
	mainIDIndex := 0
	var td *TestDirection
	_context, cancel := context.WithCancel(ctx)
	defer cancel()
	var errorTimes int64 = 0
	var requestTimes int64 = 0
	for i, server := range *ss {
		if server.ID == s.ID {
			mainIDIndex = i
		}
		sp := server
		dbg.Printf("Register Upload Handler: %s\n", sp.URL)
		td = server.Context.RegisterUploadHandler(func() {
			atomic.AddInt64(&requestTimes, 1)
			if err := uploadRequest(_context, sp, 3); err != nil {
				atomic.AddInt64(&errorTimes, 1)
			}
		})
	}
	if td == nil {
		return ErrorUninitializedManager
	}
	td.Start(ctx, cancel, mainIDIndex) // block here
	s.ULSpeed = ByteRate(td.manager.GetEWMAUploadRate())
	if s.ULSpeed == 0 && float64(errorTimes)/float64(requestTimes) > 0.1 {
		s.ULSpeed = -1 // N/A
	}
	return nil
}

// DownloadTest executes the test to measure download speed
func (s *Server) DownloadTest() error {
	return s.downloadTestContext(context.Background(), downloadRequest)
}

// DownloadTestContext executes the test to measure download speed, observing the given context.
func (s *Server) DownloadTestContext(ctx context.Context) error {
	return s.downloadTestContext(ctx, downloadRequest)
}

func (s *Server) downloadTestContext(ctx context.Context, downloadRequest downloadFunc) error {
	var errorTimes int64 = 0
	var requestTimes int64 = 0
	start := time.Now()
	_context, cancel := context.WithCancel(ctx)
	s.Context.RegisterDownloadHandler(func() {
		atomic.AddInt64(&requestTimes, 1)
		if err := downloadRequest(_context, s, 3); err != nil {
			atomic.AddInt64(&errorTimes, 1)
		}
	}).Start(ctx, cancel, 0)
	duration := time.Since(start)
	s.DLSpeed = ByteRate(s.Context.GetEWMADownloadRate())
	if s.DLSpeed == 0 && float64(errorTimes)/float64(requestTimes) > 0.1 {
		s.DLSpeed = -1 // N/A
	}
	s.TestDuration.Download = &duration
	s.testDurationTotalCount()
	return nil
}

// UploadTest executes the test to measure upload speed
func (s *Server) UploadTest() error {
	return s.uploadTestContext(context.Background(), uploadRequest)
}

// UploadTestContext executes the test to measure upload speed, observing the given context.
func (s *Server) UploadTestContext(ctx context.Context) error {
	return s.uploadTestContext(ctx, uploadRequest)
}

func (s *Server) uploadTestContext(ctx context.Context, uploadRequest uploadFunc) error {
	var errorTimes int64 = 0
	var requestTimes int64 = 0
	start := time.Now()
	_context, cancel := context.WithCancel(ctx)
	s.Context.RegisterUploadHandler(func() {
		atomic.AddInt64(&requestTimes, 1)
		if err := uploadRequest(_context, s, 4); err != nil {
			atomic.AddInt64(&errorTimes, 1)
		}
	}).Start(ctx, cancel, 0)
	duration := time.Since(start)
	s.ULSpeed = ByteRate(s.Context.GetEWMAUploadRate())
	if s.ULSpeed == 0 && float64(errorTimes)/float64(requestTimes) > 0.1 {
		s.ULSpeed = -1 // N/A
	}
	s.TestDuration.Upload = &duration
	s.testDurationTotalCount()
	return nil
}

func downloadRequest(ctx context.Context, s *Server, w int) error {
	size := dlSizes[w]
	u, err := url.Parse(s.URL)
	if err != nil {
		return err
	}
	u.Path = path.Dir(u.Path)
	xdlURL := u.JoinPath(fmt.Sprintf("random%dx%d.jpg", size, size)).String()
	dbg.Printf("XdlURL: %s\n", xdlURL)
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, xdlURL, nil)
	if err != nil {
		return err
	}

	resp, err := s.Context.doer.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	return s.Context.NewChunk().DownloadHandler(resp.Body)
}

func uploadRequest(ctx context.Context, s *Server, w int) error {
	size := ulSizes[w]
	chunkSize := int64(size*100-51) * 10
	dc := s.Context.NewChunk().UploadHandler(chunkSize)
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, s.URL, io.NopCloser(dc))
	if err != nil {
		return err
	}
	req.ContentLength = chunkSize
	dbg.Printf("Len=%d, XulURL: %s\n", req.ContentLength, s.URL)
	req.Header.Set("Content-Type", "application/octet-stream")
	resp, err := s.Context.doer.Do(req)
	if err != nil {
		return err
	}
	_, _ = io.Copy(io.Discard, resp.Body)
	defer resp.Body.Close()
	return err
}

// PingTest executes test to measure latency
func (s *Server) PingTest(callback func(latency time.Duration)) error {
	return s.PingTestContext(context.Background(), callback)
}

// PingTestContext executes test to measure latency, observing the given context.
func (s *Server) PingTestContext(ctx context.Context, callback func(latency time.Duration)) (err error) {
	start := time.Now()
	var vectorPingResult []int64
	if s.Context.config.PingMode == TCP {
		vectorPingResult, err = s.TCPPing(ctx, 10, time.Millisecond*200, callback)
	} else {
		vectorPingResult, err = s.HTTPPing(ctx, 10, time.Millisecond*200, callback)
	}
	if err != nil || len(vectorPingResult) == 0 {
		return err
	}
	dbg.Printf("Before StandardDeviation: %v\n", vectorPingResult)
	mean, _, std, minLatency, maxLatency := StandardDeviation(vectorPingResult)
	duration := time.Since(start)
	s.Latency = time.Duration(mean) * time.Nanosecond
	s.Jitter = time.Duration(std) * time.Nanosecond
	s.MinLatency = time.Duration(minLatency) * time.Nanosecond
	s.MaxLatency = time.Duration(maxLatency) * time.Nanosecond
	s.TestDuration.Ping = &duration
	s.testDurationTotalCount()
	return nil
}

// TestAll executes ping, download and upload tests one by one
func (s *Server) TestAll() error {
	err := s.PingTest(nil)
	if err != nil {
		return err
	}
	err = s.DownloadTest()
	if err != nil {
		return err
	}
	return s.UploadTest()
}

func (s *Server) TCPPing(
	ctx context.Context,
	echoTimes int,
	echoFreq time.Duration,
	callback func(latency time.Duration),
) (latencies []int64, err error) {
	var pingDst string
	if len(s.Host) == 0 {
		u, err := url.Parse(s.URL)
		if err != nil || len(u.Host) == 0 {
			return nil, err
		}
		pingDst = u.Host
	} else {
		pingDst = s.Host
	}
	failTimes := 0
	client, err := transport.NewClient(s.Context.dialFunc)
	if err != nil {
		return nil, err
	}
	err = client.Connect(ctx, pingDst)
	if err != nil {
		return nil, err
	}
	for i := 0; i < echoTimes; i++ {
		latency, err := client.PingContext(ctx)
		if err != nil {
			failTimes++
			continue
		}
		latencies = append(latencies, latency)
		if callback != nil {
			callback(time.Duration(latency))
		}
		time.Sleep(echoFreq)
	}
	if failTimes == echoTimes {
		return nil, ErrConnectTimeout
	}
	return
}

func (s *Server) HTTPPing(
	ctx context.Context,
	echoTimes int,
	echoFreq time.Duration,
	callback func(latency time.Duration),
) (latencies []int64, err error) {
	var contextErr error
	u, err := url.Parse(s.URL)
	if err != nil || len(u.Host) == 0 {
		return nil, err
	}
	u.Path = path.Dir(u.Path)
	pingDst := u.JoinPath("latency.txt").String()
	dbg.Printf("Echo: %s\n", pingDst)
	failTimes := 0
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, pingDst, nil)
	if err != nil {
		return nil, err
	}
	// carry out an extra request to warm up the connection and ensure the first request is not going to affect the
	// overall estimation
	echoTimes++
	for i := 0; i < echoTimes; i++ {
		sTime := time.Now()
		resp, err := s.Context.doer.Do(req)
		endTime := time.Since(sTime)
		if err != nil {
			if errors.Is(err, context.Canceled) || errors.Is(err, context.DeadlineExceeded) {
				contextErr = err
				break
			}

			failTimes++
			continue
		}
		_, _ = io.Copy(io.Discard, resp.Body)
		_ = resp.Body.Close()
		if i > 0 {
			latency := endTime.Nanoseconds()
			latencies = append(latencies, latency)
			dbg.Printf("RTT: %d\n", latency)
			if callback != nil {
				callback(endTime)
			}
		}
		time.Sleep(echoFreq)
	}

	if contextErr != nil {
		return latencies, contextErr
	}

	if failTimes == echoTimes {
		return nil, ErrConnectTimeout
	}

	return
}

const PingTimeout = -1

func StandardDeviation(vector []int64) (mean, variance, stdDev, min, max int64) {
	if len(vector) == 0 {
		return
	}
	var sumNum, accumulate int64
	min = math.MaxInt64
	max = math.MinInt64
	for _, value := range vector {
		sumNum += value
		if min > value {
			min = value
		}
		if max < value {
			max = value
		}
	}
	mean = sumNum / int64(len(vector))
	for _, value := range vector {
		accumulate += (value - mean) * (value - mean)
	}
	variance = accumulate / int64(len(vector))
	stdDev = int64(math.Sqrt(float64(variance)))
	return
}
