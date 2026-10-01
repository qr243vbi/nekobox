package boxbox

import (
	"context"
	"fmt"
	"io"
	"os"
	"runtime/debug"
	"time"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/adapter/endpoint"
	"github.com/sagernet/sing-box/adapter/inbound"
	"github.com/sagernet/sing-box/adapter/outbound"
	boxService "github.com/sagernet/sing-box/adapter/service"
	"github.com/sagernet/sing-box/common/taskmonitor"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/dns"
	"github.com/sagernet/sing-box/dns/transport/local"
	"github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing-box/protocol/direct"
	"github.com/sagernet/sing-box/route"
	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
	F "github.com/sagernet/sing/common/format"
	"github.com/sagernet/sing/service"
	"github.com/sagernet/sing/service/pause"
)

var _ adapter.SimpleLifecycle = (*Box)(nil)

type Box struct {
	ctx             context.Context
	createdAt       time.Time
	logFactory      log.Factory
	logger          log.ContextLogger
	network         *route.NetworkManager
	endpoint        *endpoint.Manager
	inbound         *inbound.Manager
	outbound        *outbound.Manager
	service         *boxService.Manager
	dnsTransport    *dns.TransportManager
	dnsRouter       *dns.Router
	connection      *route.ConnectionManager
	router          *route.Router
	internalService []adapter.LifecycleService
	done            chan struct{}

	StartCtx context.Context
}

type Options struct {
	option.Options
	Context           context.Context
	PlatformLogWriter log.PlatformWriter
}

func Context(
	ctx context.Context,
	inboundRegistry adapter.InboundRegistry,
	outboundRegistry adapter.OutboundRegistry,
	endpointRegistry adapter.EndpointRegistry,
	dnsTransportRegistry adapter.DNSTransportRegistry,
	serviceRegistry adapter.ServiceRegistry,
	certificateProviderRegistry adapter.CertificateProviderRegistry,
) context.Context {
	if service.FromContext[option.InboundOptionsRegistry](ctx) == nil ||
		service.FromContext[adapter.InboundRegistry](ctx) == nil {
		ctx = service.ContextWith[option.InboundOptionsRegistry](ctx, inboundRegistry)
		ctx = service.ContextWith[adapter.InboundRegistry](ctx, inboundRegistry)
	}
	if service.FromContext[option.OutboundOptionsRegistry](ctx) == nil ||
		service.FromContext[adapter.OutboundRegistry](ctx) == nil {
		ctx = service.ContextWith[option.OutboundOptionsRegistry](ctx, outboundRegistry)
		ctx = service.ContextWith[adapter.OutboundRegistry](ctx, outboundRegistry)
	}
	if service.FromContext[option.EndpointOptionsRegistry](ctx) == nil ||
		service.FromContext[adapter.EndpointRegistry](ctx) == nil {
		ctx = service.ContextWith[option.EndpointOptionsRegistry](ctx, endpointRegistry)
		ctx = service.ContextWith[adapter.EndpointRegistry](ctx, endpointRegistry)
	}
	if service.FromContext[adapter.DNSTransportRegistry](ctx) == nil {
		ctx = service.ContextWith[option.DNSTransportOptionsRegistry](ctx, dnsTransportRegistry)
		ctx = service.ContextWith[adapter.DNSTransportRegistry](ctx, dnsTransportRegistry)
	}
	if service.FromContext[adapter.ServiceRegistry](ctx) == nil {
		ctx = service.ContextWith[option.ServiceOptionsRegistry](ctx, serviceRegistry)
		ctx = service.ContextWith[adapter.ServiceRegistry](ctx, serviceRegistry)
	}
	if service.FromContext[adapter.CertificateProviderRegistry](ctx) == nil {
		ctx = service.ContextWith[option.CertificateProviderOptionsRegistry](ctx, certificateProviderRegistry)
		ctx = service.ContextWith[adapter.CertificateProviderRegistry](ctx, certificateProviderRegistry)
	}
	return ctx
}

func (b *Box) Context() context.Context {
	return b.ctx
}

func New(options Options) (*Box, error) {
	createdAt := time.Now()
	ctx := options.Context
	if ctx == nil {
		ctx = context.Background()
	}
	ctx = service.ContextWithDefaultRegistry(ctx)

	endpointRegistry := service.FromContext[adapter.EndpointRegistry](ctx)
	inboundRegistry := service.FromContext[adapter.InboundRegistry](ctx)
	outboundRegistry := service.FromContext[adapter.OutboundRegistry](ctx)
	dnsTransportRegistry := service.FromContext[adapter.DNSTransportRegistry](ctx)
	serviceRegistry := service.FromContext[adapter.ServiceRegistry](ctx)

	if endpointRegistry == nil {
		return nil, E.New("missing endpoint registry in context")
	}
	if inboundRegistry == nil {
		return nil, E.New("missing inbound registry in context")
	}
	if outboundRegistry == nil {
		return nil, E.New("missing outbound registry in context")
	}
	if dnsTransportRegistry == nil {
		return nil, E.New("missing DNS transport registry in context")
	}
	if serviceRegistry == nil {
		return nil, E.New("missing service registry in context")
	}

	ctx = pause.WithDefaultManager(ctx)

	var defaultLogWriter io.Writer = io.Discard
	logFactory, err := log.New(log.Options{
		Context:        ctx,
		Options:        common.PtrValueOrDefault(options.Log),
		BaseTime:       createdAt,
		PlatformWriter: options.PlatformLogWriter,
		DefaultWriter:  defaultLogWriter,
	})
	if err != nil {
		return nil, E.Cause(err, "create log factory")
	}

	routeOptions := common.PtrValueOrDefault(options.Route)
	dnsOptions := common.PtrValueOrDefault(options.DNS)

	endpointManager := endpoint.NewManager(logFactory.NewLogger("endpoint"), endpointRegistry)
	inboundManager := inbound.NewManager(logFactory.NewLogger("inbound"), inboundRegistry, endpointManager)
	outboundManager := outbound.NewManager(logFactory.NewLogger("outbound"), outboundRegistry, endpointManager, routeOptions.Final)
	dnsTransportManager := dns.NewTransportManager(logFactory.NewLogger("dns/transport"), dnsTransportRegistry, outboundManager, dnsOptions.Final)
	serviceManager := boxService.NewManager(logFactory.NewLogger("service"), serviceRegistry)

	service.MustRegister[adapter.EndpointManager](ctx, endpointManager)
	service.MustRegister[adapter.InboundManager](ctx, inboundManager)
	service.MustRegister[adapter.OutboundManager](ctx, outboundManager)
	service.MustRegister[adapter.DNSTransportManager](ctx, dnsTransportManager)
	service.MustRegister[adapter.ServiceManager](ctx, serviceManager)

	dnsRouter, err := dns.NewRouter(ctx, logFactory, dnsOptions)
	if err != nil {
		return nil, E.Cause(err, "initialize DNS router")
	}
	service.MustRegister[adapter.DNSRouter](ctx, dnsRouter)

	networkManager, err := route.NewNetworkManager(ctx, logFactory.NewLogger("network"), routeOptions, dnsOptions)
	if err != nil {
		return nil, E.Cause(err, "initialize network manager")
	}
	service.MustRegister[adapter.NetworkManager](ctx, networkManager)

	connectionManager := route.NewConnectionManager(logFactory.NewLogger("connection"))
	service.MustRegister[adapter.ConnectionManager](ctx, connectionManager)

	router := route.NewRouter(ctx, logFactory, "router", routeOptions, dnsOptions)
	service.MustRegister[adapter.Router](ctx, router)

	if err := router.Initialize(routeOptions.Rules, routeOptions.RuleSet); err != nil {
		return nil, E.Cause(err, "initialize router")
	}

	for i, transportOptions := range dnsOptions.Servers {
		tag := transportOptions.Tag
		if tag == "" {
			tag = F.ToString(i)
		}
		err = dnsTransportManager.Create(
			ctx,
			logFactory.NewLogger(F.ToString("dns/", transportOptions.Type, "[", tag, "]")),
			tag,
			transportOptions.Type,
			transportOptions.Options,
		)
		if err != nil {
			return nil, E.Cause(err, "initialize DNS server[", i, "]")
		}
	}

	if err := dnsRouter.Initialize(dnsOptions.Rules); err != nil {
		return nil, E.Cause(err, "initialize dns router")
	}

	for i, endpointOptions := range options.Endpoints {
		tag := endpointOptions.Tag
		if tag == "" {
			tag = F.ToString(i)
		}
		err = endpointManager.Create(
			ctx,
			router,
			logFactory.NewLogger(F.ToString("endpoint/", endpointOptions.Type, "[", tag, "]")),
			tag,
			endpointOptions.Type,
			endpointOptions.Options,
		)
		if err != nil {
			return nil, E.Cause(err, "initialize endpoint[", i, "]")
		}
	}

	for i, inboundOptions := range options.Inbounds {
		tag := inboundOptions.Tag
		if tag == "" {
			tag = F.ToString(i)
		}
		err = inboundManager.Create(
			ctx,
			router,
			logFactory.NewLogger(F.ToString("inbound/", inboundOptions.Type, "[", tag, "]")),
			tag,
			inboundOptions.Type,
			inboundOptions.Options,
		)
		if err != nil {
			return nil, E.Cause(err, "initialize inbound[", i, "]")
		}
	}

	for i, outboundOptions := range options.Outbounds {
		tag := outboundOptions.Tag
		if tag == "" {
			tag = F.ToString(i)
		}
		err = outboundManager.Create(
			ctx,
			router,
			logFactory.NewLogger(F.ToString("outbound/", outboundOptions.Type, "[", tag, "]")),
			tag,
			outboundOptions.Type,
			outboundOptions.Options,
		)
		if err != nil {
			return nil, E.Cause(err, "initialize outbound[", i, "]")
		}
	}

	for i, serviceOptions := range options.Services {
		tag := serviceOptions.Tag
		if tag == "" {
			tag = F.ToString(i)
		}
		err = serviceManager.Create(
			ctx,
			logFactory.NewLogger(F.ToString("service/", serviceOptions.Type, "[", tag, "]")),
			tag,
			serviceOptions.Type,
			serviceOptions.Options,
		)
		if err != nil {
			return nil, E.Cause(err, "initialize service[", i, "]")
		}
	}

	outboundManager.Initialize(func() (adapter.Outbound, error) {
		return direct.NewOutbound(
			ctx,
			router,
			logFactory.NewLogger("outbound/direct"),
			"direct",
			option.DirectOutboundOptions{},
		)
	})

	dnsTransportManager.Initialize(func() (adapter.DNSTransport, error) {
		return local.NewTransport(
			ctx,
			logFactory.NewLogger("dns/local"),
			"local",
			option.LocalDNSServerOptions{},
		)
	})

	return &Box{
		ctx:          ctx,
		network:      networkManager,
		endpoint:     endpointManager,
		inbound:      inboundManager,
		outbound:     outboundManager,
		dnsTransport: dnsTransportManager,
		service:      serviceManager,
		dnsRouter:    dnsRouter,
		connection:   connectionManager,
		router:       router,
		createdAt:    createdAt,
		logFactory:   logFactory,
		logger:       logFactory.Logger(),
		done:         make(chan struct{}),
	}, nil
}

func (s *Box) PreStart() error {
	err := s.preStart()
	if err != nil {
		defer func() {
			v := recover()
			if v != nil {
				println(err.Error())
				debug.PrintStack()
				panic("panic on early close: " + fmt.Sprint(v))
			}
		}()
		s.Close()
		return err
	}
	s.logger.Info("sing-box pre-started (", F.Seconds(time.Since(s.createdAt).Seconds()), "s)")
	return nil
}

func (s *Box) Start() error {
	err := s.start()
	if err != nil {
		defer func() {
			v := recover()
			if v != nil {
				println(err.Error())
				debug.PrintStack()
				println("panic on early start: " + fmt.Sprint(v))
			}
		}()
		s.Close()
		return err
	}
	s.logger.Info("sing-box started (", F.Seconds(time.Since(s.createdAt).Seconds()), "s)")
	return nil
}

func (s *Box) preStart() error {
	monitor := taskmonitor.New(s.logger, C.StartTimeout)
	monitor.Start("start logger")
	err := s.logFactory.Start()
	monitor.Finish()
	if err != nil {
		return E.Cause(err, "start logger")
	}
	err = adapter.StartNamed(s.ctx, s.logger, adapter.StartStateInitialize, s.internalService)
	if err != nil {
		return err
	}
	err = adapter.Start(s.ctx, s.logger, adapter.StartStateInitialize, s.network, s.dnsTransport, s.dnsRouter, s.connection, s.router, s.outbound, s.inbound, s.endpoint, s.service)
	if err != nil {
		return err
	}
	err = adapter.Start(s.ctx, s.logger, adapter.StartStateStart, s.outbound, s.dnsTransport, s.dnsRouter, s.network, s.connection, s.router)
	if err != nil {
		return err
	}
	return nil
}

func (s *Box) start() error {
	err := s.preStart()
	if err != nil {
		return err
	}
	err = adapter.StartNamed(s.ctx, s.logger, adapter.StartStateStart, s.internalService)
	if err != nil {
		return err
	}
	err = adapter.Start(s.ctx, s.logger, adapter.StartStateStart, s.inbound, s.endpoint, s.service)
	if err != nil {
		return err
	}
	err = adapter.Start(s.ctx, s.logger, adapter.StartStatePostStart, s.outbound, s.network, s.dnsTransport, s.dnsRouter, s.connection, s.router, s.inbound, s.endpoint, s.service)
	if err != nil {
		return err
	}
	err = adapter.StartNamed(s.ctx, s.logger, adapter.StartStatePostStart, s.internalService)
	if err != nil {
		return err
	}
	err = adapter.Start(s.ctx, s.logger, adapter.StartStateStarted, s.network, s.dnsTransport, s.dnsRouter, s.connection, s.router, s.outbound, s.inbound, s.endpoint, s.service)
	if err != nil {
		return err
	}
	err = adapter.StartNamed(s.ctx, s.logger, adapter.StartStateStarted, s.internalService)
	if err != nil {
		return err
	}
	return nil
}

func (s *Box) Close() error {
	select {
	case <-s.done:
		return os.ErrClosed
	default:
		close(s.done)
	}
	err := common.Close(
		s.service, s.endpoint, s.inbound, s.outbound, s.router, s.connection, s.dnsRouter, s.dnsTransport, s.network,
	)
	for _, lifecycleService := range s.internalService {
		err = E.Append(err, lifecycleService.Close(), func(err error) error {
			return E.Cause(err, "close ", lifecycleService.Name())
		})
	}
	err = E.Append(err, s.logFactory.Close(), func(err error) error {
		return E.Cause(err, "close logger")
	})
	return err
}

func (s *Box) Network() adapter.NetworkManager {
	return s.network
}

func (s *Box) Router() adapter.Router {
	return s.router
}

func (s *Box) Inbound() adapter.InboundManager {
	return s.inbound
}

func (s *Box) Outbound() adapter.OutboundManager {
	return s.outbound
}