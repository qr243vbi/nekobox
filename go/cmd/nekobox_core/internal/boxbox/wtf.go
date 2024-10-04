package boxbox

import (
	"github.com/sagernet/sing-box/experimental/clashapi"
	E "github.com/sagernet/sing/common/exceptions"
)

func (s *Box) closeClashApi() error {
	if c, ok := s.router.ClashServer().(*clashapi.Server); ok {
		return c.Close()
	}
	return nil
}

func (s *Box) closeInboundListeners() error {
	var errors error
	for i, in := range s.inbounds {
		inType := in.Type()
		if inType == "tun" {
			continue
		}
		s.logger.Trace("closeInboundListener inbound/", inType, "[", i, "]")
		errors = E.Append(errors, in.Close(), func(err error) error {
			return E.Cause(err, "closeInboundListener inbound/", inType, "[", i, "]")
		})
	}
	return errors
}
