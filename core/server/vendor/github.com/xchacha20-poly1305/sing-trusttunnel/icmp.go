package trusttunnel

import (
	"encoding/binary"
	"net/netip"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
)

type IcmpConn struct {
	httpConn
}

func (i *IcmpConn) WritePing(id uint16, destination netip.Addr, sequenceNumber uint16, ttl uint8, size uint16) error {
	request := buf.NewSize(2 + 16 + 2 + 1 + 2)
	defer request.Release()
	common.Must(binary.Write(request, binary.BigEndian, id))
	destinationAddress := buildPaddingIP(destination)
	common.Must1(request.Write(destinationAddress[:]))
	common.Must(binary.Write(request, binary.BigEndian, sequenceNumber))
	common.Must(binary.Write(request, binary.BigEndian, ttl))
	common.Must(binary.Write(request, binary.BigEndian, size))
	return common.Error(i.writeFlush(request.Bytes()))
}

func (i *IcmpConn) ReadPing() (id uint16, sourceAddress netip.Addr, icmpType uint8, code uint8, sequenceNumber uint16, err error) {
	err = i.waitCreated()
	if err != nil {
		return
	}
	response := buf.NewSize(2 + 16 + 1 + 1 + 2)
	defer response.Release()
	_, err = response.ReadFullFrom(i.body, response.Cap())
	if err != nil {
		err = i.wrapError(err)
		return
	}
	common.Must(binary.Read(response, binary.BigEndian, &id))
	var sourceAddressBuffer [16]byte
	common.Must1(response.Read(sourceAddressBuffer[:]))
	sourceAddress = parse16BytesIP(sourceAddressBuffer)
	common.Must(binary.Read(response, binary.BigEndian, &icmpType))
	common.Must(binary.Read(response, binary.BigEndian, &code))
	common.Must(binary.Read(response, binary.BigEndian, &sequenceNumber))
	return
}

func (i *IcmpConn) Close() error {
	return i.httpConn.Close()
}

func (i *IcmpConn) ReadPingRequest() (id uint16, destination netip.Addr, sequenceNumber uint16, ttl uint8, size uint16, err error) {
	err = i.waitCreated()
	if err != nil {
		return
	}
	request := buf.NewSize(2 + 16 + 2 + 1 + 2)
	defer request.Release()
	_, err = request.ReadFullFrom(i.body, request.Cap())
	if err != nil {
		err = i.wrapError(err)
		return
	}
	common.Must(binary.Read(request, binary.BigEndian, &id))
	var destinationAddressBuffer [16]byte
	common.Must1(request.Read(destinationAddressBuffer[:]))
	destination = parse16BytesIP(destinationAddressBuffer)
	common.Must(binary.Read(request, binary.BigEndian, &sequenceNumber))
	common.Must(binary.Read(request, binary.BigEndian, &ttl))
	common.Must(binary.Read(request, binary.BigEndian, &size))
	return
}

func (i *IcmpConn) WritePingResponse(id uint16, sourceAddress netip.Addr, icmpType uint8, code uint8, sequenceNumber uint16) error {
	response := buf.NewSize(2 + 16 + 1 + 1 + 2)
	defer response.Release()
	common.Must(binary.Write(response, binary.BigEndian, id))
	sourceAddressBytes := buildPaddingIP(sourceAddress)
	common.Must1(response.Write(sourceAddressBytes[:]))
	common.Must(binary.Write(response, binary.BigEndian, icmpType))
	common.Must(binary.Write(response, binary.BigEndian, code))
	common.Must(binary.Write(response, binary.BigEndian, sequenceNumber))
	return common.Error(i.writeFlush(response.Bytes()))
}
