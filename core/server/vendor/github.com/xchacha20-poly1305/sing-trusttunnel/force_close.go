package trusttunnel

import (
	"net"
	"net/http"
	"reflect"
	"sync"
	"unsafe"

	"github.com/sagernet/sing/common"

	"golang.org/x/net/http2"
)

func forceCloseAllConnections(roundTripper RoundTripper) {
	roundTripper.CloseIdleConnections()
	_ = common.Close(roundTripper) // Can close http3 connections
	if h2Transport, isH2Transport := roundTripper.(*http2.Transport); isH2Transport {
		connPool := transportConnPool(h2Transport)
		p := (*h2ClientConnPool)((*efaceWords)(unsafe.Pointer(&connPool)).data)
		p.mu.Lock()
		defer p.mu.Unlock()
		for _, clientConns := range p.conns {
			for _, clientConn := range clientConns {
				_ = clientConn.Close()
			}
		}
		return
	}
}

type efaceWords struct {
	typ  unsafe.Pointer
	data unsafe.Pointer
}

//go:linkname transportConnPool golang.org/x/net/http2.(*Transport).connPool
func transportConnPool(t *http2.Transport) http2.ClientConnPool

type h2ClientConnPool struct {
	t *http2.Transport

	mu    sync.Mutex
	conns map[string][]*http2.ClientConn // key is host:port
	/*dialing      map[string]*dialCall     // currently in-flight dials
	keys         map[*ClientConn][]string
	addConnCalls map[string]*addConnCall // in-flight addConnIfNeeded calls*/
}

func forceCloseAllH2ServerConnections(server *http2.Server) {
	if server == nil {
		return
	}
	state := h2ServerState(server)
	if state == nil {
		return
	}
	state.mu.Lock()
	serverConns := make([]*h2ServerConn, 0, len(state.activeConns))
	for serverConn := range state.activeConns {
		serverConns = append(serverConns, serverConn)
	}
	state.mu.Unlock()
	for _, serverConn := range serverConns {
		if serverConn != nil && serverConn.conn != nil {
			_ = serverConn.conn.Close()
		}
	}
}

type h2ServerInternalState struct {
	mu          sync.Mutex
	activeConns map[*h2ServerConn]struct{}
}

type h2ServerConn struct {
	srv  *http2.Server
	hs   *http.Server
	conn net.Conn
}

func h2ServerState(server *http2.Server) *h2ServerInternalState {
	stateField, loaded := reflect.TypeFor[http2.Server]().FieldByName("state")
	if !loaded || stateField.Type.Kind() != reflect.Pointer {
		return nil
	}
	return *(**h2ServerInternalState)(unsafe.Add(unsafe.Pointer(server), stateField.Offset))
}
