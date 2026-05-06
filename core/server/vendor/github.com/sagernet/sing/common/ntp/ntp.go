package ntp

import (
	"context"
	"crypto/rand"
	"encoding/binary"
	"time"

	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

func Exchange(ctx context.Context, dialer N.Dialer, serverAddress M.Socksaddr) (*Response, error) {
	conn, err := dialer.DialContext(ctx, N.NetworkUDP, serverAddress)
	if err != nil {
		return nil, err
	}
	defer conn.Close()
	conn.SetDeadline(time.Now().Add(defaultTimeout))

	var request msg
	request.setMode(client)
	request.setVersion(defaultNtpVersion)
	request.setLeap(LeapNotInSync)

	bits := make([]byte, 8)
	_, err = rand.Read(bits)
	var xmitTime time.Time
	if err == nil {
		request.TransmitTime = ntpTime(binary.BigEndian.Uint64(bits))
		xmitTime = time.Now()
	} else {
		xmitTime = time.Now()
		request.TransmitTime = toNtpTime(xmitTime)
	}

	err = binary.Write(conn, binary.BigEndian, request)
	if err != nil {
		return nil, err
	}

	var response msg
	err = binary.Read(conn, binary.BigEndian, &response)
	if err != nil {
		return nil, err
	}

	recvTime := toNtpTime(xmitTime.Add(time.Since(xmitTime)))
	response.OriginTime = toNtpTime(xmitTime)
	return parseTime(&response, recvTime), nil
}
