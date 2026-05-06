package bufio

import (
	"io"

	N "github.com/sagernet/sing/common/network"
)

type CopyOptions struct {
	ReadWaitOptions     N.ReadWaitOptions
	IncreaseBufferAfter int64
	ReadCounters        []N.CountFunc
	WriteCounters       []N.CountFunc
	Handshake           N.HandshakeState
}

type CopySession struct {
	source       io.Reader
	destination  io.Writer
	originSource io.Reader
	options      CopyOptions
	onTransfer   func(n int64) error
}

func NewCopySession(destination io.Writer, source io.Reader, originSource io.Reader, options CopyOptions) *CopySession {
	session := &CopySession{
		source:       source,
		destination:  destination,
		originSource: originSource,
		options:      options,
	}
	session.resetTransferHook()
	return session
}

func (s *CopySession) ResetHandshake() {
	s.options.Handshake = N.NewHandshakeState(s.source, s.destination)
	s.resetTransferHook()
}

func (s *CopySession) Transfer(n int64) error {
	if s.onTransfer == nil {
		return nil
	}
	return s.onTransfer(n)
}

func (s *CopySession) resetTransferHook() {
	s.onTransfer = newTransferHook(s.options.Handshake, s.options.ReadCounters, s.options.WriteCounters)
}

func newTransferHook(handshake N.HandshakeState, readCounters []N.CountFunc, writeCounters []N.CountFunc) func(n int64) error {
	upgradable := handshake.Upgradable()
	if len(readCounters) == 0 && len(writeCounters) == 0 && !upgradable {
		return nil
	}
	return func(n int64) error {
		for _, counter := range readCounters {
			counter(n)
		}
		for _, counter := range writeCounters {
			counter(n)
		}
		if upgradable {
			return handshake.Check()
		}
		return nil
	}
}
