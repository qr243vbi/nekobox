package network

import (
	"errors"
	"io"

	"github.com/sagernet/sing/common"
)

var ErrHandshakeCompleted = errors.New("protocol handshake completed")

// Deprecated: use EarlyReader and EarlyWriter instead.
type EarlyConn interface {
	NeedHandshake() bool
}

type EarlyReader interface {
	NeedHandshakeForRead() bool
}

func NeedHandshakeForRead(reader io.Reader) bool {
	if earlyReader, isEarlyReader := common.Cast[EarlyReader](reader); isEarlyReader && earlyReader.NeedHandshakeForRead() {
		return true
	}
	return false
}

type EarlyWriter interface {
	NeedHandshakeForWrite() bool
}

func NeedHandshakeForWrite(writer io.Writer) bool {
	if //goland:noinspection GoDeprecation
	earlyConn, isEarlyConn := writer.(EarlyConn); isEarlyConn {
		return earlyConn.NeedHandshake()
	}
	if earlyWriter, isEarlyWriter := common.Cast[EarlyWriter](writer); isEarlyWriter && earlyWriter.NeedHandshakeForWrite() {
		return true
	}
	return false
}

type HandshakeState struct {
	readPending  bool
	writePending bool
	source       io.Reader
	destination  io.Writer
}

func NewHandshakeState(source io.Reader, destination io.Writer) HandshakeState {
	return HandshakeState{
		readPending:  NeedHandshakeForRead(source),
		writePending: NeedHandshakeForWrite(destination),
		source:       source,
		destination:  destination,
	}
}

func (s HandshakeState) Upgradable() bool {
	return s.readPending || s.writePending
}

func (s HandshakeState) Check() error {
	if s.readPending && !NeedHandshakeForRead(s.source) {
		return ErrHandshakeCompleted
	}
	if s.writePending && !NeedHandshakeForWrite(s.destination) {
		return ErrHandshakeCompleted
	}
	return nil
}
