package xhttp

import (
	"bytes"
	"context"
	"encoding/base64"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/http/httptrace"
	"strings"
	"sync"

	common "github.com/sagernet/sing-box/common/xray"
	"github.com/sagernet/sing-box/common/vision"
	"github.com/sagernet/sing-box/common/xray/signal/done"
	"github.com/sagernet/sing-box/option"
)

// interface to abstract between use of browser dialer, vs net/http
type DialerClient interface {
	IsClosed() bool

	OpenStream(context.Context, string, string, io.Reader, bool) (io.ReadCloser, net.Addr, net.Addr, error)
	PostPacket(context.Context, string, string, string, io.Reader, int64) error
}

// implements xhttp.DialerClient in terms of direct network connections
type DefaultDialerClient struct {
	options     *option.V2RayXHTTPBaseOptions
	client      *http.Client
	closed      bool
	httpVersion string
	// pool of net.Conn, created using dialUploadConn
	uploadRawPool  *sync.Pool
	dialUploadConn func(ctxInner context.Context) (net.Conn, error)
}

func (c *DefaultDialerClient) IsClosed() bool {
	return c.closed
}

func (c *DefaultDialerClient) OpenStream(ctx context.Context, url string, sessionId string, body io.Reader, uploadOnly bool) (wrc io.ReadCloser, remoteAddr, localAddr net.Addr, err error) {
	gotConn := done.New()
	ctx = httptrace.WithClientTrace(ctx, &httptrace.ClientTrace{
		GotConn: func(connInfo httptrace.GotConnInfo) {
			remoteAddr = connInfo.Conn.RemoteAddr()
			localAddr = connInfo.Conn.LocalAddr()
			if hook, ok := vision.HookFromContext(ctx); ok {
				hook(connInfo.Conn)
			}
			gotConn.Close()
		},
	})
	method := "GET"
	if body != nil {
		method = c.options.GetNormalizedUplinkHTTPMethod()
	}
	req, _ := http.NewRequestWithContext(context.WithoutCancel(ctx), method, url, body)
	req.Header = c.options.GetRequestHeader()
	length := int(c.options.GetNormalizedXPaddingBytes().Rand())
	config := XPaddingConfig{Length: length}
	if c.options.XPaddingObfsMode {
		config.Placement = XPaddingPlacement{
			Placement: c.options.XPaddingPlacement,
			Key:       c.options.XPaddingKey,
			Header:    c.options.XPaddingHeader,
			RawURL:    url,
		}
		config.Method = PaddingMethod(c.options.XPaddingMethod)
	} else {
		config.Placement = XPaddingPlacement{
			Placement: option.PlacementQueryInHeader,
			Key:       "x_padding",
			Header:    "Referer",
			RawURL:    url,
		}
		config.Method = PaddingMethodRepeatX
	}
	ApplyXPaddingToRequest(req, config)
	ApplyMetaToRequest(c.options, req, sessionId, "")
	if method == c.options.GetNormalizedUplinkHTTPMethod() && !c.options.NoGRPCHeader {
		req.Header.Set("Content-Type", "application/grpc")
	}
	wrc = &WaitReadCloser{Wait: make(chan struct{})}
	go func() {
		resp, err := c.client.Do(req)
		if err != nil {
			if !uploadOnly {
				c.closed = true
			}
			gotConn.Close()
			wrc.Close()
			return
		}
		if resp.StatusCode != 200 || uploadOnly {
			if resp.StatusCode != 200 {
				c.closed = true
			}
			io.Copy(io.Discard, resp.Body)
			resp.Body.Close()
			wrc.Close()
			return
		}
		wrc.(*WaitReadCloser).Set(resp.Body)
	}()
	<-gotConn.Wait()
	return
}

func (c *DefaultDialerClient) PostPacket(ctx context.Context, url string, sessionId string, seqStr string, body io.Reader, contentLength int64) error {
	var encodedData string
	dataPlacement := c.options.GetNormalizedUplinkDataPlacement()
	if dataPlacement != option.PlacementBody {
		data, err := io.ReadAll(body)
		if err != nil {
			return err
		}
		encodedData = base64.RawURLEncoding.EncodeToString(data)
		body = nil
		contentLength = 0
	}
	method := c.options.GetNormalizedUplinkHTTPMethod()
	req, err := http.NewRequestWithContext(context.WithoutCancel(ctx), method, url, body)
	if err != nil {
		return err
	}
	req.ContentLength = contentLength
	req.Header = c.options.GetRequestHeader()
	if dataPlacement != option.PlacementBody {
		key := c.options.UplinkDataKey
		chunkSize := int(c.options.UplinkChunkSize)
		switch dataPlacement {
		case option.PlacementHeader:
			for i := 0; i < len(encodedData); i += chunkSize {
				end := i + chunkSize
				if end > len(encodedData) {
					end = len(encodedData)
				}
				chunk := encodedData[i:end]
				headerKey := fmt.Sprintf("%s-%d", key, i/chunkSize)
				req.Header.Set(headerKey, chunk)
			}
			req.Header.Set(key+"-Length", fmt.Sprintf("%d", len(encodedData)))
			req.Header.Set(key+"-Upstream", "1")
		case option.PlacementCookie:
			for i := 0; i < len(encodedData); i += chunkSize {
				end := i + chunkSize
				if end > len(encodedData) {
					end = len(encodedData)
				}
				chunk := encodedData[i:end]
				cookieName := fmt.Sprintf("%s_%d", key, i/chunkSize)
				req.AddCookie(&http.Cookie{Name: cookieName, Value: chunk})
			}
			req.AddCookie(&http.Cookie{Name: key + "_upstream", Value: "1"})
		}
	}
	length := int(c.options.GetNormalizedXPaddingBytes().Rand())
	config := XPaddingConfig{Length: length}
	if c.options.XPaddingObfsMode {
		config.Placement = XPaddingPlacement{
			Placement: c.options.XPaddingPlacement,
			Key:       c.options.XPaddingKey,
			Header:    c.options.XPaddingHeader,
			RawURL:    url,
		}
		config.Method = PaddingMethod(c.options.XPaddingMethod)
	} else {
		config.Placement = XPaddingPlacement{
			Placement: option.PlacementQueryInHeader,
			Key:       "x_padding",
			Header:    "Referer",
			RawURL:    url,
		}
		config.Method = PaddingMethodRepeatX
	}
	ApplyXPaddingToRequest(req, config)
	ApplyMetaToRequest(c.options, req, sessionId, seqStr)
	if c.httpVersion != "1.1" {
		resp, err := c.client.Do(req)
		if err != nil {
			c.closed = true
			return err
		}
		_, copyErr := io.Copy(io.Discard, resp.Body)
		closeErr := resp.Body.Close()
		if resp.StatusCode != 200 {
			c.closed = true
			if copyErr != nil {
				return copyErr
			}
			if closeErr != nil {
				return closeErr
			}
			return fmt.Errorf("bad status code: %s", resp.Status)
		}
		if copyErr != nil {
			return copyErr
		}
		if closeErr != nil {
			return closeErr
		}
	} else {
		requestBuff := new(bytes.Buffer)
		common.Must(req.Write(requestBuff))
		var uploadConn any
		var h1UploadConn *H1Conn
		for {
			uploadConn = c.uploadRawPool.Get()
			newConnection := uploadConn == nil
			if newConnection {
				newConn, err := c.dialUploadConn(context.WithoutCancel(ctx))
				if err != nil {
					return err
				}
				h1UploadConn = NewH1Conn(newConn)
				uploadConn = h1UploadConn
			} else {
				h1UploadConn = uploadConn.(*H1Conn)
				if h1UploadConn.UnreadedResponsesCount > 0 {
					resp, err := http.ReadResponse(h1UploadConn.RespBufReader, req)
					if err != nil {
						c.closed = true
						return fmt.Errorf("error while reading response: %s", err.Error())
					}
					_, copyErr := io.Copy(io.Discard, resp.Body)
					closeErr := resp.Body.Close()
					if resp.StatusCode != 200 {
						c.closed = true
						return fmt.Errorf("got non-200 error response code: %d", resp.StatusCode)
					}
					if copyErr != nil {
						return copyErr
					}
					if closeErr != nil {
						return closeErr
					}
				}
			}
			_, err := h1UploadConn.Write(requestBuff.Bytes())
			if err == nil {
				break
			} else if newConnection {
				return err
			}
		}
		c.uploadRawPool.Put(uploadConn)
	}
	return nil
}

type WaitReadCloser struct {
	Wait chan struct{}
	io.ReadCloser
	mu   sync.Mutex
	once sync.Once
	closed bool
}

func (w *WaitReadCloser) notify() {
	w.once.Do(func() {
		close(w.Wait)
	})
}

func (w *WaitReadCloser) Set(rc io.ReadCloser) {
	w.mu.Lock()
	if w.closed || w.ReadCloser != nil {
		w.mu.Unlock()
		rc.Close()
		return
	}
	w.ReadCloser = rc
	w.mu.Unlock()
	w.notify()
}

func (w *WaitReadCloser) Read(b []byte) (int, error) {
	w.mu.Lock()
	rc := w.ReadCloser
	w.mu.Unlock()

	if rc == nil {
		<-w.Wait
		w.mu.Lock()
		rc = w.ReadCloser
		w.mu.Unlock()
		if rc == nil {
			return 0, io.ErrClosedPipe
		}
	}
	return rc.Read(b)
}

func (w *WaitReadCloser) Close() error {
	w.mu.Lock()
	if w.closed {
		w.mu.Unlock()
		return nil
	}
	w.closed = true
	rc := w.ReadCloser
	w.ReadCloser = nil
	w.mu.Unlock()

	if rc != nil {
		return rc.Close()
	}

	w.notify()
	return nil
}

func ApplyMetaToRequest(options *option.V2RayXHTTPBaseOptions, req *http.Request, sessionId string, seqStr string) {
	sessionPlacement := options.GetNormalizedSessionPlacement()
	seqPlacement := options.GetNormalizedSeqPlacement()
	sessionKey := options.GetNormalizedSessionKey()
	seqKey := options.GetNormalizedSeqKey()
	if sessionId != "" {
		switch sessionPlacement {
		case option.PlacementPath:
			req.URL.Path = appendToPath(req.URL.Path, sessionId)
		case option.PlacementQuery:
			q := req.URL.Query()
			q.Set(sessionKey, sessionId)
			req.URL.RawQuery = q.Encode()
		case option.PlacementHeader:
			req.Header.Set(sessionKey, sessionId)
		case option.PlacementCookie:
			req.AddCookie(&http.Cookie{Name: sessionKey, Value: sessionId})
		}
	}
	if seqStr != "" {
		switch seqPlacement {
		case option.PlacementPath:
			req.URL.Path = appendToPath(req.URL.Path, seqStr)
		case option.PlacementQuery:
			q := req.URL.Query()
			q.Set(seqKey, seqStr)
			req.URL.RawQuery = q.Encode()
		case option.PlacementHeader:
			req.Header.Set(seqKey, seqStr)
		case option.PlacementCookie:
			req.AddCookie(&http.Cookie{Name: seqKey, Value: seqStr})
		}
	}
}

func appendToPath(path, value string) string {
	if strings.HasSuffix(path, "/") {
		return path + value
	}
	return path + "/" + value
}
