package ntp

import (
	"context"
	"os"
	"time"

	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/x/list"
	"github.com/sagernet/sing/service"
	"github.com/sagernet/sing/service/pause"
)

const TimeLayout = "2006-01-02 15:04:05 -0700"

type TimeService interface {
	TimeFunc() func() time.Time
}

type Options struct {
	Context       context.Context
	Dialer        N.Dialer
	Logger        logger.Logger
	Server        M.Socksaddr
	Interval      time.Duration
	Timeout       time.Duration
	WriteToSystem bool
}

var _ TimeService = (*Service)(nil)

type Service struct {
	ctx           context.Context
	cancel        common.ContextCancelCauseFunc
	dialer        N.Dialer
	logger        logger.Logger
	server        M.Socksaddr
	writeToSystem bool
	ticker        *time.Ticker
	interval      time.Duration
	timeout       time.Duration
	clockOffset   time.Duration
	pause         pause.Manager
	pauseCallback *list.Element[pause.Callback]
}

func NewService(options Options) *Service {
	ctx := options.Context
	if ctx == nil {
		ctx = context.Background()
	}
	ctx, cancel := common.ContextWithCancelCause(ctx)
	destination := options.Server
	if !destination.IsValid() {
		destination = M.Socksaddr{
			Fqdn: "time.apple.com",
		}
	}
	if options.Logger == nil {
		options.Logger = logger.NOP()
	}
	if destination.Port == 0 {
		destination.Port = 123
	}
	var interval time.Duration
	if options.Interval > 0 {
		interval = options.Interval
	} else {
		interval = 30 * time.Minute
	}
	var dialer N.Dialer
	if options.Dialer != nil {
		dialer = options.Dialer
	} else {
		dialer = N.SystemDialer
	}
	return &Service{
		ctx:           ctx,
		cancel:        cancel,
		dialer:        dialer,
		logger:        options.Logger,
		writeToSystem: options.WriteToSystem,
		server:        destination,
		interval:      interval,
		timeout:       options.Timeout,
		pause:         service.FromContext[pause.Manager](ctx),
	}
}

func (s *Service) Start() error {
	err := s.update()
	if err != nil {
		s.logger.Error(E.Cause(err, "initialize time"))
	} else {
		s.logger.Info("updated time: ", s.TimeFunc()().Local().Format(TimeLayout))
	}
	s.ticker = time.NewTicker(s.interval)
	go s.loopUpdate()
	if s.pause != nil {
		s.pauseCallback = pause.RegisterTicker(s.pause, s.ticker, s.interval, s.updateOnce)
	}
	return nil
}

func (s *Service) Close() error {
	if s.ticker != nil {
		s.ticker.Stop()
	}
	s.cancel(os.ErrClosed)
	return nil
}

func (s *Service) TimeFunc() func() time.Time {
	return func() time.Time {
		return time.Now().Add(s.clockOffset)
	}
}

func (s *Service) loopUpdate() {
	for {
		select {
		case <-s.ctx.Done():
			return
		case <-s.ticker.C:
		}
		s.updateOnce()
	}
}

func (s *Service) updateOnce() {
	err := s.update()
	if err == nil {
		s.logger.Info("updated time: ", s.TimeFunc()().Local().Format(TimeLayout))
	} else {
		s.logger.Error("update time: ", err)
	}
}

func (s *Service) update() error {
	ctx := s.ctx
	var cancel context.CancelFunc
	if s.timeout > 0 {
		ctx, cancel = context.WithTimeout(ctx, s.timeout)
	}
	response, err := Exchange(ctx, s.dialer, s.server)
	if cancel != nil {
		cancel()
	}
	if err != nil {
		return err
	}
	s.clockOffset = response.ClockOffset
	if s.writeToSystem {
		writeErr := SetSystemTime(s.TimeFunc()())
		if writeErr != nil {
			s.logger.Error("write time to system: ", writeErr)
		}
	}
	return nil
}
