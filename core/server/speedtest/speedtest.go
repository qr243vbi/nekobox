package speedtest

import (
	"context"
	"errors"
	"fmt"
	"net"
	"net/http"
	"runtime"
	"time"
)

var (
	version          = "1.7.10"
	DefaultUserAgent = fmt.Sprintf("showwin/speedtest-go %s", version)
)

type Proto int

const (
	HTTP Proto = iota
	TCP
)

// Speedtest is a speedtest client.
type Speedtest struct {
	User *User
	Manager

	dialFunc func(ctx context.Context, network string, address string) (net.Conn, error)
	doer     *http.Client
	config   *UserConfig
}

type UserConfig struct {
	T               *http.Transport
	DialContextFunc func(ctx context.Context, network string, address string) (net.Conn, error)
	Debug           bool
	PingMode        Proto

	MaxConnections int

	CityFlag     string
	LocationFlag string
	Location     *Location

	Keyword string // Fuzzy search
}

func (s *Speedtest) NewUserConfig(uc *UserConfig) {
	if uc.DialContextFunc == nil {
		panic("DialContextFunc is nil")
	}

	if uc.Debug {
		dbg.Enable()
	}

	s.SetNThread(uc.MaxConnections)

	if len(uc.CityFlag) > 0 {
		var err error
		uc.Location, err = GetLocation(uc.CityFlag)
		if err != nil {
			dbg.Printf("Warning: skipping command line arguments: --city. err: %v\n", err.Error())
		}
	}
	if len(uc.LocationFlag) > 0 {
		var err error
		uc.Location, err = ParseLocation(uc.CityFlag, uc.LocationFlag)
		if err != nil {
			dbg.Printf("Warning: skipping command line arguments: --location. err: %v\n", err.Error())
		}
	}
	s.dialFunc = uc.DialContextFunc
	s.config = uc

	s.config.T = &http.Transport{
		DialContext:           uc.DialContextFunc,
		ForceAttemptHTTP2:     true,
		MaxIdleConns:          100,
		IdleConnTimeout:       90 * time.Second,
		TLSHandshakeTimeout:   10 * time.Second,
		ExpectContinueTimeout: 1 * time.Second,
	}

	s.doer.Transport = s
}

func (s *Speedtest) RoundTrip(req *http.Request) (*http.Response, error) {
	req.Header.Add("User-Agent", DefaultUserAgent)
	return s.config.T.RoundTrip(req)
}

// Option is a function that can be passed to New to modify the Client.
type Option func(*Speedtest)

// WithUserConfig adds a custom user config for speedtest.
// This configuration may be overwritten again by WithDoer,
// because client and transport are parent-child relationship:
// `New(WithDoer(myDoer), WithUserAgent(myUserAgent), WithDoer(myDoer))`
func WithUserConfig(userConfig *UserConfig) Option {
	return func(s *Speedtest) {
		s.NewUserConfig(userConfig)
		dbg.Printf("Keyword: %v\n", s.config.Keyword)
		dbg.Printf("PingType: %v\n", s.config.PingMode)
		dbg.Printf("OS: %s, ARCH: %s, NumCPU: %d\n", runtime.GOOS, runtime.GOARCH, runtime.NumCPU())
	}
}

func cloneDefaultClient() *http.Client {
	newClient := *http.DefaultClient
	newClient.Transport = &http.Transport{
		Proxy:                 http.ProxyFromEnvironment,
		DialContext:           nil,
		ForceAttemptHTTP2:     true,
		MaxIdleConns:          100,
		IdleConnTimeout:       90 * time.Second,
		TLSHandshakeTimeout:   10 * time.Second,
		ExpectContinueTimeout: 1 * time.Second,
	}

	return &newClient
}

// New creates a new speedtest client.
func New(opts ...Option) *Speedtest {
	s := &Speedtest{
		doer:    cloneDefaultClient(),
		Manager: NewDataManager(),
	}
	// load default config
	s.NewUserConfig(&UserConfig{DialContextFunc: func(ctx context.Context, network string, address string) (net.Conn, error) {
		return nil, errors.New("no dial func")
	}})

	for _, opt := range opts {
		opt(s)
	}
	return s
}

func Version() string {
	return version
}

var defaultClient = New()
