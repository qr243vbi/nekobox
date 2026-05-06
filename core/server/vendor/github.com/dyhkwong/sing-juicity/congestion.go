/*
Copyright (C) 2025  dyhkwong

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
*/

package juicity

import (
	"context"
	"time"

	"github.com/sagernet/quic-go"
	"github.com/sagernet/quic-go/congestion"
	"github.com/sagernet/sing-quic/congestion_bbr1"
	"github.com/sagernet/sing-quic/congestion_bbr2"
	congestion_meta1 "github.com/sagernet/sing-quic/congestion_meta1"
	congestion_meta2 "github.com/sagernet/sing-quic/congestion_meta2"
	"github.com/sagernet/sing/common/ntp"
)

func setCongestion(ctx context.Context, connection *quic.Conn, congestionName string) {
	timeFunc := ntp.TimeFuncFromContext(ctx)
	if timeFunc == nil {
		timeFunc = time.Now
	}
	switch congestionName {
	case "cubic":
		connection.SetCongestionControl(
			congestion_meta1.NewCubicSender(
				congestion_meta1.DefaultClock{TimeFunc: timeFunc},
				congestion.ByteCount(connection.Config().InitialPacketSize),
				false,
			),
		)
	case "new_reno":
		connection.SetCongestionControl(
			congestion_meta1.NewCubicSender(
				congestion_meta1.DefaultClock{TimeFunc: timeFunc},
				congestion.ByteCount(connection.Config().InitialPacketSize),
				true,
			),
		)
	case "bbr_meta_v1":
		connection.SetCongestionControl(congestion_meta1.NewBBRSender(
			congestion_meta1.DefaultClock{TimeFunc: timeFunc},
			congestion.ByteCount(connection.Config().InitialPacketSize),
			congestion_meta1.InitialCongestionWindow*congestion_meta1.InitialMaxDatagramSize,
			congestion_meta1.DefaultBBRMaxCongestionWindow*congestion_meta1.InitialMaxDatagramSize,
		))
	case "bbr":
		connection.SetCongestionControl(congestion_meta2.NewBbrSender(
			congestion_meta2.DefaultClock{TimeFunc: timeFunc},
			congestion.ByteCount(connection.Config().InitialPacketSize),
			congestion.ByteCount(congestion_meta1.InitialCongestionWindow),
		))
	case "bbr_quiche":
		connection.SetCongestionControl(congestion_bbr1.NewBbrSender(
			congestion_bbr1.DefaultClock{TimeFunc: timeFunc},
			congestion.ByteCount(connection.Config().InitialPacketSize),
			congestion_bbr1.InitialCongestionWindowPackets,
			congestion_bbr1.MaxCongestionWindowPackets,
		))
	case "bbr2":
		connection.SetCongestionControl(congestion_bbr2.NewBBR2Sender(
			congestion_bbr2.DefaultClock{TimeFunc: timeFunc},
			congestion.ByteCount(connection.Config().InitialPacketSize),
			0,
			false,
		))
	case "bbr2_aggressive":
		connection.SetCongestionControl(congestion_bbr2.NewBBR2Sender(
			congestion_bbr2.DefaultClock{TimeFunc: timeFunc},
			congestion.ByteCount(connection.Config().InitialPacketSize),
			32*congestion.ByteCount(connection.Config().InitialPacketSize),
			true,
		))
	}
}
