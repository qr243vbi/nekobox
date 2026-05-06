package netmon

import (
	N "github.com/sagernet/sing/common/network"
)

func (m *Monitor) Dialer() N.Dialer {
	return m.dialer
}
