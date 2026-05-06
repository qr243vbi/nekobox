package hysteria

import (
	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
)

func (s *serverSession[U]) loopMessages() {
	for {
		message, err := s.quicConn.ReceiveDatagram(s.ctx)
		if err != nil {
			s.closeWithError(E.Cause(err, "receive message"))
			return
		}
		hErr := s.handleMessage(message)
		if hErr != nil {
			s.closeWithError(E.Cause(hErr, "handle message"))
			return
		}
	}
}

func (s *serverSession[U]) handleMessage(data []byte) error {
	message := allocMessage()
	err := decodeUDPMessage(message, data)
	if err != nil {
		message.release()
		return E.Cause(err, "decode UDP message")
	}
	return s.handleUDPMessage(message)
}

func (s *serverSession[U]) handleUDPMessage(message *udpMessage) error {
	s.udpAccess.RLock()
	udpConn, loaded := s.udpConnMap[message.sessionID]
	s.udpAccess.RUnlock()
	if !loaded || common.Done(udpConn.ctx) {
		message.release()
		return E.New("unknown session iD: ", message.sessionID)
	}
	udpConn.inputPacket(message)
	return nil
}
