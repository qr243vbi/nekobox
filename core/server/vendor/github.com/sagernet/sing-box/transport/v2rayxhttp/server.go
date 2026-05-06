package xhttp

import (
	"bytes"
	"context"
	"encoding/base64"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/sagernet/quic-go"
	"github.com/sagernet/quic-go/http3"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/common/tls"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing-box/option"
	qtls "github.com/sagernet/sing-quic"

	"github.com/sagernet/sing-box/common/xray/signal/done"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/logger"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	aTLS "github.com/sagernet/sing/common/tls"
	sHttp "github.com/sagernet/sing/protocol/http"
)

var _ adapter.V2RayServerTransport = (*Server)(nil)

type Server struct {
	ctx         context.Context
	logger      logger.ContextLogger
	tlsConfig   tls.ServerConfig
	quicConfig  *quic.Config
	handler     adapter.V2RayServerTransportHandler
	httpServer  *http.Server
	http3Server *http3.Server
	localAddr   net.Addr
	options     *option.V2RayXHTTPOptions
	host        string
	path        string
	sessionMu   sync.Mutex
	sessions    sync.Map
	enableTCP   bool
	enableH3    bool
}

func NewServer(ctx context.Context, logger logger.ContextLogger, options option.V2RayXHTTPOptions, tlsConfig tls.ServerConfig, handler adapter.V2RayServerTransportHandler) (*Server, error) {
	mode, err := option.NormalizeXHTTPMode(options.Mode)
	if err != nil {
		return nil, err
	}
	options.Mode = mode
	server := &Server{
		ctx:       ctx,
		logger:    logger,
		tlsConfig: tlsConfig,
		handler:   handler,
		options:   &options,
		host:      options.Host,
		path:      options.GetNormalizedPath(),
	}
	hasNonH3 := true
	if tlsConfig != nil {
		hasNonH3 = false
		for _, proto := range tlsConfig.NextProtos() {
			if proto == "h3" {
				server.enableH3 = true
			} else if proto != "" {
				hasNonH3 = true
			}
		}
		if len(tlsConfig.NextProtos()) == 0 {
			hasNonH3 = true
		}
	} else {
		server.enableH3 = false
	}
	server.enableTCP = hasNonH3
	if server.enableTCP {
		protocols := new(http.Protocols)
		protocols.SetHTTP1(true)
		protocols.SetUnencryptedHTTP2(true)
		server.httpServer = &http.Server{
			Handler:           server,
			ReadHeaderTimeout: time.Second * 4,
			MaxHeaderBytes:    8192,
			Protocols:         protocols,
			BaseContext: func(net.Listener) context.Context {
				return ctx
			},
			ConnContext: func(ctx context.Context, c net.Conn) context.Context {
				return log.ContextWithNewID(ctx)
			},
		}
	}
	if server.enableH3 {
		server.quicConfig = &quic.Config{
			DisablePathMTUDiscovery: !C.IsLinux && !C.IsWindows,
		}
		server.http3Server = &http3.Server{
			Handler: server,
		}
	}
	return server, nil
}

func (s *Server) ServeHTTP(writer http.ResponseWriter, request *http.Request) {
	if len(s.host) > 0 && !isValidHTTPHost(request.Host, s.host) {
		s.logger.ErrorContext(request.Context(), "failed to validate host, request:", request.Host, ", config:", s.host)
		writer.WriteHeader(http.StatusNotFound)
		return
	}
	if !strings.HasPrefix(request.URL.Path, s.path) {
		s.logger.ErrorContext(request.Context(), "failed to validate path, request:", request.URL.Path, ", config:", s.path)
		writer.WriteHeader(http.StatusNotFound)
		return
	}
	writer.Header().Set("Access-Control-Allow-Origin", "*")
	writer.Header().Set("Access-Control-Allow-Methods", "*")
	length := int(s.options.GetNormalizedXPaddingBytes().Rand())
	config := XPaddingConfig{Length: length}
	if s.options.XPaddingObfsMode {
		config.Placement = XPaddingPlacement{
			Placement: s.options.XPaddingPlacement,
			Key:       s.options.XPaddingKey,
			Header:    s.options.XPaddingHeader,
		}
		config.Method = PaddingMethod(s.options.XPaddingMethod)
	} else {
		config.Placement = XPaddingPlacement{
			Placement: option.PlacementHeader,
			Header:    "X-Padding",
		}
	}
	ApplyXPaddingToHeader(writer.Header(), config)
	validRange := s.options.GetNormalizedXPaddingBytes()
	paddingValue, paddingPlacement := ExtractXPaddingFromRequest(&s.options.V2RayXHTTPBaseOptions, request, s.options.XPaddingObfsMode)
	if !IsPaddingValid(&s.options.V2RayXHTTPBaseOptions, paddingValue, validRange.From, validRange.To, PaddingMethod(s.options.XPaddingMethod)) {
		s.logger.ErrorContext(request.Context(), "invalid padding ("+paddingPlacement+") length:", int32(len(paddingValue)))
		writer.WriteHeader(http.StatusBadRequest)
		return
	}
	sessionId, seqStr := ExtractMetaFromRequest(s.options, request, s.path)
	if sessionId == "" && s.options.Mode != "" && s.options.Mode != "auto" && s.options.Mode != "stream-one" && s.options.Mode != "stream-up" {
		s.logger.ErrorContext(request.Context(), "stream-one mode is not allowed")
		writer.WriteHeader(http.StatusBadRequest)
		return
	}
	forwardedAddrs := parseXForwardedFor(request.Header)
	var remoteAddr net.Addr
	var err error
	remoteAddr, err = net.ResolveTCPAddr("tcp", request.RemoteAddr)
	if err != nil {
		remoteAddr = &net.TCPAddr{
			IP:   []byte{0, 0, 0, 0},
			Port: 0,
		}
	}
	if request.ProtoMajor == 3 {
		remoteAddr = &net.UDPAddr{
			IP:   remoteAddr.(*net.TCPAddr).IP,
			Port: remoteAddr.(*net.TCPAddr).Port,
		}
	}
	if len(forwardedAddrs) > 0 && forwardedAddrs[0].Family().IsIP() {
		remoteAddr = &net.TCPAddr{
			IP:   forwardedAddrs[0].IP(),
			Port: 0,
		}
	}
	var currentSession *httpSession
	if sessionId != "" {
		currentSession = s.upsertSession(sessionId)
	}
	scMaxEachPostBytes := int(s.options.GetNormalizedScMaxEachPostBytes().To)
	uplinkHTTPMethod := s.options.GetNormalizedUplinkHTTPMethod()
	isUplinkRequest := false
	if uplinkHTTPMethod != "GET" && request.Method == uplinkHTTPMethod {
		isUplinkRequest = true
	}
	uplinkDataPlacement := s.options.GetNormalizedUplinkDataPlacement()
	uplinkDataKey := s.options.UplinkDataKey
	switch uplinkDataPlacement {
	case option.PlacementHeader:
		if request.Header.Get(uplinkDataKey+"-Upstream") == "1" {
			isUplinkRequest = true
		}
	case option.PlacementCookie:
		if c, _ := request.Cookie(uplinkDataKey + "_upstream"); c != nil && c.Value == "1" {
			isUplinkRequest = true
		}
	}
	if isUplinkRequest && sessionId != "" {
		if seqStr == "" {
			if s.options.Mode != "" && s.options.Mode != "auto" && s.options.Mode != "stream-up" {
				s.logger.ErrorContext(request.Context(), "stream-up mode is not allowed")
				writer.WriteHeader(http.StatusBadRequest)
				return
			}
			httpSC := &httpServerConn{
				Instance:       done.New(),
				Reader:         request.Body,
				ResponseWriter: writer,
			}
			err = currentSession.uploadQueue.Push(Packet{
				Reader: httpSC,
			})
			if err != nil {
				s.logger.InfoContext(request.Context(), err, "failed to upload (PushReader)")
				writer.WriteHeader(http.StatusConflict)
			} else {
				writer.Header().Set("X-Accel-Buffering", "no")
				writer.Header().Set("Cache-Control", "no-store")
				writer.WriteHeader(http.StatusOK)
				scStreamUpServerSecs := s.options.GetNormalizedScStreamUpServerSecs()
				referrer := request.Header.Get("Referer")
				if referrer != "" && scStreamUpServerSecs.To > 0 {
					go func() {
						for {
							_, err := httpSC.Write(bytes.Repeat([]byte{'X'}, int(s.options.GetNormalizedXPaddingBytes().Rand())))
							if err != nil {
								break
							}
							time.Sleep(time.Duration(scStreamUpServerSecs.Rand()) * time.Second)
						}
					}()
				}
				select {
				case <-request.Context().Done():
				case <-httpSC.Wait():
				}
			}
			httpSC.Close()
			return
		}
		if s.options.Mode != "" && s.options.Mode != "auto" && s.options.Mode != "packet-up" {
			s.logger.ErrorContext(request.Context(), "packet-up mode is not allowed")
			writer.WriteHeader(http.StatusBadRequest)
			return
		}
		var payload []byte
		if uplinkDataPlacement != option.PlacementBody {
			var encodedStr string
			switch uplinkDataPlacement {
			case option.PlacementHeader:
				dataLenStr := request.Header.Get(uplinkDataKey + "-Length")
				if dataLenStr != "" {
					dataLen, _ := strconv.Atoi(dataLenStr)
					var chunks []string
					i := 0
					for {
						chunk := request.Header.Get(fmt.Sprintf("%s-%d", uplinkDataKey, i))
						if chunk == "" {
							break
						}
						chunks = append(chunks, chunk)
						i++
					}
					encodedStr = strings.Join(chunks, "")
					if len(encodedStr) != dataLen {
						encodedStr = ""
					}
				}
			case option.PlacementCookie:
				var chunks []string
				i := 0
				for {
					cookieName := fmt.Sprintf("%s_%d", uplinkDataKey, i)
					if c, _ := request.Cookie(cookieName); c != nil {
						chunks = append(chunks, c.Value)
						i++
					} else {
						break
					}
				}
				if len(chunks) > 0 {
					encodedStr = strings.Join(chunks, "")
				}
			}
			if encodedStr != "" {
				payload, err = base64.RawURLEncoding.DecodeString(encodedStr)
			} else {
				s.logger.ErrorContext(request.Context(), err, "failed to extract data from key "+uplinkDataKey+" placed in "+uplinkDataPlacement)
				writer.WriteHeader(http.StatusInternalServerError)
				return
			}
		} else {
			payload, err = io.ReadAll(io.LimitReader(request.Body, int64(scMaxEachPostBytes)+1))
		}
		if len(payload) > scMaxEachPostBytes {
			s.logger.ErrorContext(request.Context(), "Too large upload. scMaxEachPostBytes is set to ", scMaxEachPostBytes, "but request size exceed it. Adjust scMaxEachPostBytes on the server to be at least as large as client.")
			writer.WriteHeader(http.StatusRequestEntityTooLarge)
			return
		}
		if err != nil {
			s.logger.InfoContext(request.Context(), err, "failed to upload (ReadAll)")
			writer.WriteHeader(http.StatusInternalServerError)
			return
		}
		seq, err := strconv.ParseUint(seqStr, 10, 64)
		if err != nil {
			s.logger.InfoContext(request.Context(), err, "failed to upload (ParseUint)")
			writer.WriteHeader(http.StatusInternalServerError)
			return
		}
		err = currentSession.uploadQueue.Push(Packet{
			Payload: payload,
			Seq:     seq,
		})
		if err != nil {
			s.logger.InfoContext(request.Context(), err, "failed to upload (PushPayload)")
			writer.WriteHeader(http.StatusInternalServerError)
			return
		}
		writer.WriteHeader(http.StatusOK)
	} else if request.Method == "GET" || sessionId == "" {
		if sessionId != "" {
			currentSession.isFullyConnected.Close()
			defer s.sessions.Delete(sessionId)
		}
		writer.Header().Set("X-Accel-Buffering", "no")
		writer.Header().Set("Cache-Control", "no-store")
		if !s.options.NoSSEHeader {
			writer.Header().Set("Content-Type", "text/event-stream")
		}
		writer.WriteHeader(http.StatusOK)
		writer.(http.Flusher).Flush()
		httpSC := &httpServerConn{
			Instance:       done.New(),
			Reader:         request.Body,
			ResponseWriter: writer,
		}
		conn := splitConn{
			writer:     httpSC,
			reader:     httpSC,
			remoteAddr: remoteAddr,
			localAddr:  s.localAddr,
		}
		if sessionId != "" {
			conn.reader = currentSession.uploadQueue
		}
		s.handler.NewConnectionEx(request.Context(), &conn, sHttp.SourceAddress(request), M.Socksaddr{}, func(it error) {})
		select {
		case <-request.Context().Done():
		case <-httpSC.Wait():
		}
		conn.Close()
	} else {
		s.logger.ErrorContext(request.Context(), "unsupported method: ", request.Method)
		writer.WriteHeader(http.StatusMethodNotAllowed)
	}
}

func (s *Server) Network() []string {
	var networks []string
	if s.enableTCP {
		networks = append(networks, N.NetworkTCP)
	}
	if s.enableH3 {
		networks = append(networks, N.NetworkUDP)
	}
	return networks
}

func (s *Server) Serve(listener net.Listener) error {
	if !s.enableTCP {
		return os.ErrInvalid
	}
	if s.tlsConfig != nil {
		listener = aTLS.NewListener(listener, s.tlsConfig)
	}
	s.localAddr = listener.Addr()
	return s.httpServer.Serve(listener)
}

func (s *Server) ServePacket(listener net.PacketConn) error {
	if !s.enableH3 {
		return os.ErrInvalid
	}
	quicListener, err := qtls.ListenEarly(listener, s.tlsConfig, s.quicConfig)
	if err != nil {
		return err
	}
	s.localAddr = quicListener.Addr()
	return s.http3Server.ServeListener(quicListener)
}

func (s *Server) Close() error {
	var closers []any
	if s.enableTCP {
		closers = append(closers, s.httpServer)
	}
	if s.enableH3 {
		closers = append(closers, s.http3Server)
	}
	return common.Close(closers...)
}

func (s *Server) upsertSession(sessionId string) *httpSession {
	currentSessionAny, ok := s.sessions.Load(sessionId)
	if ok {
		return currentSessionAny.(*httpSession)
	}
	s.sessionMu.Lock()
	defer s.sessionMu.Unlock()
	currentSessionAny, ok = s.sessions.Load(sessionId)
	if ok {
		return currentSessionAny.(*httpSession)
	}
	session := &httpSession{
		uploadQueue:      NewUploadQueue(s.options.GetNormalizedScMaxBufferedPosts()),
		isFullyConnected: done.New(),
	}
	s.sessions.Store(sessionId, session)
	shouldReap := done.New()
	go func() {
		time.Sleep(30 * time.Second)
		shouldReap.Close()
	}()
	go func() {
		select {
		case <-shouldReap.Wait():
			s.sessions.Delete(sessionId)
			session.uploadQueue.Close()
		case <-session.isFullyConnected.Wait():
		}
	}()
	return session
}

func ExtractMetaFromRequest(options *option.V2RayXHTTPOptions, req *http.Request, path string) (sessionId string, seqStr string) {
	sessionPlacement := options.GetNormalizedSessionPlacement()
	seqPlacement := options.GetNormalizedSeqPlacement()
	sessionKey := options.GetNormalizedSessionKey()
	seqKey := options.GetNormalizedSeqKey()
	if sessionPlacement == option.PlacementPath && seqPlacement == option.PlacementPath {
		subpath := strings.Split(req.URL.Path[len(path):], "/")
		if len(subpath) > 0 {
			sessionId = subpath[0]
		}
		if len(subpath) > 1 {
			seqStr = subpath[1]
		}
		return sessionId, seqStr
	}
	switch sessionPlacement {
	case option.PlacementQuery:
		sessionId = req.URL.Query().Get(sessionKey)
	case option.PlacementHeader:
		sessionId = req.Header.Get(sessionKey)
	case option.PlacementCookie:
		if cookie, e := req.Cookie(sessionKey); e == nil {
			sessionId = cookie.Value
		}
	}
	switch seqPlacement {
	case option.PlacementQuery:
		seqStr = req.URL.Query().Get(seqKey)
	case option.PlacementHeader:
		seqStr = req.Header.Get(seqKey)
	case option.PlacementCookie:
		if cookie, e := req.Cookie(seqKey); e == nil {
			seqStr = cookie.Value
		}
	}
	return sessionId, seqStr
}
