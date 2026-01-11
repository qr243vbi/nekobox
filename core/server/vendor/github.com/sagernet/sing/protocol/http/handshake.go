package http

import (
	std_bufio "bufio"
	"context"
	"encoding/base64"
	"io"
	"net"
	"net/http"
	"strings"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/auth"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	F "github.com/sagernet/sing/common/format"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/pipe"
)

func HandleConnectionEx(
	ctx context.Context,
	conn net.Conn,
	reader *std_bufio.Reader,
	authenticator *auth.Authenticator,
	handler N.TCPConnectionHandlerEx,
	source M.Socksaddr,
	onClose N.CloseHandlerFunc,
) error {
	for {
		request, err := ReadRequest(reader)
		if err != nil {
			return E.Cause(err, "read http request")
		}
		if authenticator != nil {
			var (
				username string
				password string
				authOk   bool
			)
			authorization := request.Header.Get("Proxy-Authorization")
			if strings.HasPrefix(authorization, "Basic ") {
				userPassword, _ := base64.URLEncoding.DecodeString(authorization[6:])
				userPswdArr := strings.SplitN(string(userPassword), ":", 2)
				if len(userPswdArr) == 2 {
					username = userPswdArr[0]
					password = userPswdArr[1]
					authOk = authenticator.Verify(username, password)
					if authOk {
						ctx = auth.ContextWithUser(ctx, userPswdArr[0])
					}
				}
			}
			if !authOk {
				// Since no one else is using the library, use a fixed realm until rewritten
				err = responseWith(
					request, http.StatusProxyAuthRequired,
					"Proxy-Authenticate", `Basic realm="sing-box" charset="UTF-8"`,
				).Write(conn)
				if err != nil {
					return err
				}
				if username != "" {
					return E.New("http: authentication failed, username=", username, ", password=", password)
				} else if authorization != "" {
					return E.New("http: authentication failed, Proxy-Authorization=", authorization)
				} else {
					return E.New("http: authentication failed, no Proxy-Authorization header")
				}
			}
		}

		if sourceAddress := SourceAddress(request); sourceAddress.IsValid() {
			source = sourceAddress
		}

		if request.Method == "CONNECT" {
			destination := M.ParseSocksaddrHostPortStr(request.URL.Hostname(), request.URL.Port()).Unwrap()
			if destination.Port == 0 {
				switch request.URL.Scheme {
				case "https", "wss":
					destination.Port = 443
				default:
					destination.Port = 80
				}
			}
			_, err = conn.Write([]byte(F.ToString("HTTP/", request.ProtoMajor, ".", request.ProtoMinor, " 200 Connection established\r\n\r\n")))
			if err != nil {
				return E.Cause(err, "write http response")
			}
			var requestConn net.Conn
			if reader.Buffered() > 0 {
				buffer := buf.NewSize(reader.Buffered())
				_, err = buffer.ReadFullFrom(reader, reader.Buffered())
				if err != nil {
					return err
				}
				requestConn = bufio.NewCachedConn(conn, buffer)
			} else {
				requestConn = conn
			}
			handler.NewConnectionEx(ctx, requestConn, source, destination, onClose)
			return nil
		} else if strings.ToLower(request.Header.Get("Connection")) == "upgrade" {
			destination := M.ParseSocksaddrHostPortStr(request.URL.Hostname(), request.URL.Port()).Unwrap()
			if destination.Port == 0 {
				switch request.URL.Scheme {
				case "https", "wss":
					destination.Port = 443
				default:
					destination.Port = 80
				}
			}
			serverConn, clientConn := pipe.Pipe()
			go func() {
				handler.NewConnectionEx(ctx, clientConn, source, destination, func(it error) {
					if it != nil {
						common.Close(serverConn, clientConn)
					}
				})
			}()
			err = request.Write(serverConn)
			if err != nil {
				return E.Cause(err, "http: write upgrade request")
			}
			if reader.Buffered() > 0 {
				_, err = io.CopyN(serverConn, reader, int64(reader.Buffered()))
				if err != nil {
					return err
				}
			}
			return bufio.CopyConn(ctx, conn, serverConn)
		} else {
			err = handleHTTPConnection(ctx, handler, conn, request, source)
			if err != nil {
				return err
			}
		}
	}
}

func handleHTTPConnection(
	ctx context.Context,
	handler N.TCPConnectionHandlerEx,
	conn net.Conn,
	request *http.Request, source M.Socksaddr,
) error {
	keepAlive := !(request.ProtoMajor == 1 && request.ProtoMinor == 0) && strings.TrimSpace(strings.ToLower(request.Header.Get("Proxy-Connection"))) == "keep-alive"
	request.RequestURI = ""

	removeHopByHopHeaders(request.Header)
	removeExtraHTTPHostPort(request)

	if hostStr := request.Header.Get("Host"); hostStr != "" {
		if hostStr != request.URL.Host {
			request.Host = hostStr
		}
	}

	if request.URL.Scheme == "" || request.URL.Host == "" {
		return responseWith(request, http.StatusBadRequest).Write(conn)
	}

	var innerErr common.TypedValue[error]
	httpClient := &http.Client{
		Transport: &http.Transport{
			DisableCompression: true,
			DialContext: func(ctx context.Context, network, address string) (net.Conn, error) {
				input, output := pipe.Pipe()
				go handler.NewConnectionEx(ctx, output, source, M.ParseSocksaddr(address).Unwrap(), func(it error) {
					innerErr.Store(it)
					common.Close(input, output)
				})
				return input, nil
			},
		},
		CheckRedirect: func(req *http.Request, via []*http.Request) error {
			return http.ErrUseLastResponse
		},
	}
	defer httpClient.CloseIdleConnections()

	requestCtx, cancel := context.WithCancel(ctx)
	response, err := httpClient.Do(request.WithContext(requestCtx))
	if err != nil {
		cancel()
		return E.Errors(innerErr.Load(), err, responseWith(request, http.StatusBadGateway).Write(conn))
	}

	removeHopByHopHeaders(response.Header)

	if keepAlive {
		response.Header.Set("Proxy-Connection", "keep-alive")
		response.Header.Set("Connection", "keep-alive")
		response.Header.Set("Keep-Alive", "timeout=4")
	}

	response.Close = !keepAlive

	err = response.Write(conn)
	if err != nil {
		cancel()
		return E.Errors(innerErr.Load(), err)
	}

	cancel()
	if !keepAlive {
		return conn.Close()
	}
	return nil
}

func removeHopByHopHeaders(header http.Header) {
	// Strip hop-by-hop header based on RFC:
	// http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html#sec13.5.1
	// https://www.mnot.net/blog/2011/07/11/what_proxies_must_do

	header.Del("Proxy-Connection")
	header.Del("Proxy-Authenticate")
	header.Del("Proxy-Authorization")
	header.Del("TE")
	header.Del("Trailers")
	header.Del("Transfer-Encoding")
	header.Del("Upgrade")

	connections := header.Get("Connection")
	header.Del("Connection")
	if len(connections) == 0 {
		return
	}
	for _, h := range strings.Split(connections, ",") {
		header.Del(strings.TrimSpace(h))
	}
}

func removeExtraHTTPHostPort(req *http.Request) {
	host := req.Host
	if host == "" {
		host = req.URL.Host
	}

	if pHost, port, err := net.SplitHostPort(host); err == nil && port == "80" {
		if M.ParseAddr(pHost).Is6() {
			pHost = "[" + pHost + "]"
		}
		host = pHost
	}

	req.Host = host
	req.URL.Host = host
}

func responseWith(request *http.Request, statusCode int, headers ...string) *http.Response {
	var header http.Header
	if len(headers) > 0 {
		header = make(http.Header)
		for i := 0; i < len(headers); i += 2 {
			header.Add(headers[i], headers[i+1])
		}
	}
	return &http.Response{
		StatusCode: statusCode,
		Status:     http.StatusText(statusCode),
		Proto:      request.Proto,
		ProtoMajor: request.ProtoMajor,
		ProtoMinor: request.ProtoMinor,
		Header:     header,
	}
}
