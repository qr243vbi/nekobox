// Package tfo provides TCP Fast Open support for the [net] dialer and listener.
//
// The dial functions have an additional buffer parameter, which specifies data in SYN.
// If the buffer is empty, TFO is not used.
//
// This package supports Linux, Windows, macOS, and FreeBSD.
// On unsupported platforms, [ErrPlatformUnsupported] is returned.
//
// FreeBSD code is completely untested. Use at your own risk. Feedback is welcome.
package tfo

import (
	"context"
	"errors"
	"net"
	"sync/atomic"
	"time"
)

var (
	ErrPlatformUnsupported PlatformUnsupportedError
	errMissingAddress      = errors.New("missing address")
)

// PlatformUnsupportedError is returned when tfo-go does not support TCP Fast Open on the current platform.
type PlatformUnsupportedError struct{}

func (PlatformUnsupportedError) Error() string {
	return "tfo-go does not support TCP Fast Open on this platform"
}

func (PlatformUnsupportedError) Is(target error) bool {
	return target == errors.ErrUnsupported
}

var runtimeListenNoTFO atomic.Bool

// ListenConfig wraps [net.ListenConfig] with TFO-related options.
type ListenConfig struct {
	net.ListenConfig

	// Backlog specifies the maximum number of pending TFO connections on supported platforms.
	// If the value is 0, Go std's listen(2) backlog is used.
	// If the value is negative, TFO is disabled.
	Backlog int

	// DisableTFO controls whether TCP Fast Open is disabled when the Listen method is called.
	// TFO is enabled by default, unless [ListenConfig.Backlog] is negative.
	// Set to true to disable TFO and it will behave exactly the same as [net.ListenConfig].
	DisableTFO bool

	// Fallback controls whether to proceed without TFO when TFO is enabled but not supported
	// on the system.
	Fallback bool
}

func (lc *ListenConfig) tfoDisabled() bool {
	return lc.Backlog < 0 || lc.DisableTFO
}

func (lc *ListenConfig) tfoNeedsFallback() bool {
	return lc.Fallback && (comptimeListenNoTFO || runtimeListenNoTFO.Load())
}

// TFO returns true if the next Listen call will attempt to enable TFO.
func (lc *ListenConfig) TFO() bool {
	return !lc.tfoDisabled() && !lc.tfoNeedsFallback()
}

// Listen is like [net.ListenConfig.Listen] but enables TFO whenever possible,
// unless [ListenConfig.Backlog] is negative or [ListenConfig.DisableTFO] is set to true.
func (lc *ListenConfig) Listen(ctx context.Context, network, address string) (net.Listener, error) {
	if lc.tfoDisabled() || !networkIsTCP(network) || lc.tfoNeedsFallback() {
		return lc.ListenConfig.Listen(ctx, network, address)
	}
	return lc.listenTFO(ctx, network, address) // tfo_darwin.go, tfo_listen_generic.go, tfo_listen_stub.go
}

// ListenContext is like [net.ListenContext] but enables TFO whenever possible.
func ListenContext(ctx context.Context, network, address string) (net.Listener, error) {
	var lc ListenConfig
	return lc.Listen(ctx, network, address)
}

// Listen is like [net.Listen] but enables TFO whenever possible.
func Listen(network, address string) (net.Listener, error) {
	return ListenContext(context.Background(), network, address)
}

// ListenTCP is like [net.ListenTCP] but enables TFO whenever possible.
func ListenTCP(network string, laddr *net.TCPAddr) (*net.TCPListener, error) {
	if !networkIsTCP(network) {
		return nil, &net.OpError{Op: "listen", Net: network, Source: nil, Addr: opAddr(laddr), Err: net.UnknownNetworkError(network)}
	}
	var address string
	if laddr != nil {
		address = laddr.String()
	}
	var lc ListenConfig
	ln, err := lc.listenTFO(context.Background(), network, address) // tfo_darwin.go, tfo_listen_generic.go, tfo_listen_stub.go
	if err != nil {
		return nil, err
	}
	return ln.(*net.TCPListener), err
}

type dialTFOSupport uint32

const (
	dialTFOSupportDefault dialTFOSupport = iota
	dialTFOSupportNone
	dialTFOSupportLinuxSendto
)

type atomicDialTFOSupport struct {
	v atomic.Uint32
}

func (a *atomicDialTFOSupport) load() dialTFOSupport {
	return dialTFOSupport(a.v.Load())
}

func (a *atomicDialTFOSupport) storeNone() {
	a.v.Store(uint32(dialTFOSupportNone))
}

var runtimeDialTFOSupport atomicDialTFOSupport

// Dialer wraps [net.Dialer] with an additional option that allows you to disable TFO.
type Dialer struct {
	net.Dialer

	// DisableTFO controls whether TCP Fast Open is disabled when the dial methods are called.
	// TFO is enabled by default.
	// Set to true to disable TFO and it will behave exactly the same as [net.Dialer].
	DisableTFO bool

	// Fallback controls whether to proceed without TFO when TFO is enabled but not supported
	// on the system.
	// On Linux this also controls whether the sendto(MSG_FASTOPEN) fallback path is tried
	// before giving up on TFO.
	Fallback bool
}

func (d *Dialer) dialAndWrite(ctx context.Context, network, address string, b []byte) (net.Conn, error) {
	c, err := d.Dialer.DialContext(ctx, network, address)
	if err != nil {
		return nil, err
	}
	if err = netConnWriteBytes(ctx, c, b); err != nil {
		c.Close()
		return nil, err
	}
	return c, nil
}

func (d *Dialer) dialAndWriteTCPConn(ctx context.Context, network, address string, b []byte) (*net.TCPConn, error) {
	c, err := d.Dialer.DialContext(ctx, network, address)
	if err != nil {
		return nil, err
	}
	if err = netConnWriteBytes(ctx, c, b); err != nil {
		c.Close()
		return nil, err
	}
	return c.(*net.TCPConn), nil
}

// TFO returns true if the next dial call will attempt to enable TFO.
func (d *Dialer) TFO() bool {
	return !d.DisableTFO && (!d.Fallback || !comptimeDialNoTFO && runtimeDialTFOSupport.load() != dialTFOSupportNone)
}

// DialContext is like [net.Dialer.DialContext] but enables TFO whenever possible,
// unless [Dialer.DisableTFO] is set to true.
func (d *Dialer) DialContext(ctx context.Context, network, address string, b []byte) (net.Conn, error) {
	if len(b) == 0 {
		return d.Dialer.DialContext(ctx, network, address)
	}
	if d.DisableTFO || !networkIsTCP(network) {
		return d.dialAndWrite(ctx, network, address, b)
	}
	tc, err := d.dialTFO(ctx, network, address, b) // tfo_bsd+windows.go, tfo_connect_stub.go, tfo_linux.go
	if err != nil {
		return nil, err // return nil [net.Conn] instead of non-nil [net.Conn] with nil [*net.TCPConn] pointer
	}
	return tc, nil
}

// Dial is like [net.Dialer.Dial] but enables TFO whenever possible,
// unless [Dialer.DisableTFO] is set to true.
func (d *Dialer) Dial(network, address string, b []byte) (net.Conn, error) {
	return d.DialContext(context.Background(), network, address, b)
}

// Dial is like [net.Dial] but enables TFO whenever possible.
func Dial(network, address string, b []byte) (net.Conn, error) {
	var d Dialer
	return d.DialContext(context.Background(), network, address, b)
}

// DialTimeout is like [net.DialTimeout] but enables TFO whenever possible.
func DialTimeout(network, address string, timeout time.Duration, b []byte) (net.Conn, error) {
	var d Dialer
	d.Timeout = timeout
	return d.DialContext(context.Background(), network, address, b)
}

// DialTCP is like [net.DialTCP] but enables TFO whenever possible.
func DialTCP(network string, laddr, raddr *net.TCPAddr, b []byte) (*net.TCPConn, error) {
	if len(b) == 0 {
		return net.DialTCP(network, laddr, raddr)
	}
	if !networkIsTCP(network) {
		return nil, &net.OpError{Op: "dial", Net: network, Source: opAddr(laddr), Addr: opAddr(raddr), Err: net.UnknownNetworkError(network)}
	}
	if raddr == nil {
		return nil, &net.OpError{Op: "dial", Net: network, Source: opAddr(laddr), Addr: nil, Err: errMissingAddress}
	}
	return dialTCPAddr(network, laddr, raddr, b) // tfo_bsd+windows.go, tfo_connect_stub.go, tfo_linux.go
}

func networkIsTCP(network string) bool {
	switch network {
	case "tcp", "tcp4", "tcp6":
		return true
	default:
		return false
	}
}

func opAddr(a *net.TCPAddr) net.Addr {
	if a == nil {
		return nil
	}
	return a
}

// aLongTimeAgo is a non-zero time, far in the past, used for immediate deadlines.
var aLongTimeAgo = time.Unix(0, 0)

// writeDeadliner allows cancellation of ongoing write operations.
type writeDeadliner interface {
	SetWriteDeadline(t time.Time) error
}

// connWriteFunc invokes the given function on a [writeDeadliner] to execute any arbitrary write operation.
// If the given context can be canceled, it will spin up an interruptor goroutine to cancel the write operation
// when the context is canceled.
func connWriteFunc[C writeDeadliner](ctx context.Context, c C, fn func(C) error) (err error) {
	stop := context.AfterFunc(ctx, func() {
		_ = c.SetWriteDeadline(aLongTimeAgo)
	})
	defer func() {
		if !stop() && err == nil {
			err = ctx.Err()
		}
	}()
	return fn(c)
}

// netConnWriteBytes is a convenience wrapper around [connWriteFunc] for writing bytes to a [net.Conn].
func netConnWriteBytes(ctx context.Context, c net.Conn, b []byte) error {
	return connWriteFunc(ctx, c, func(c net.Conn) error {
		_, err := c.Write(b)
		return err
	})
}
