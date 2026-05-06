/*
Copyright (C) 2025 dyhkwong

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
	"github.com/sagernet/sing/common/metadata"
)

const (
	Version = 0
)

const (
	CommandAuthenticate = 0
)

const (
	NetworkTCP = 1
	NetworkUDP = 3
)

const AuthenticateLen = 2 + 16 + 32

// Juicity Specification tells us it is 0x01 for IPv4, 0x02 for IPv6, and 0x03 for domain, which is incorrect.
var AddressSerializer = metadata.NewSerializer(
	metadata.AddressFamilyByte(0x01, metadata.AddressFamilyIPv4),
	metadata.AddressFamilyByte(0x04, metadata.AddressFamilyIPv6),
	metadata.AddressFamilyByte(0x03, metadata.AddressFamilyFqdn),
)
