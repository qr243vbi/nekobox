package internal

import (
	"encoding/binary"
	"math/bits"
)

func GetType(bytes []byte) uint32 {
	var b1 [4]byte
	b1[0] = bytes[0] / 4
	ll := bytes[0] % 4
	copy(bytes[1:1+ll], b1[1:1+ll])

	v1 := binary.LittleEndian.Uint32(b1[:])
	return v1
}

func GetId(bytes []byte) (uint64, uint64) {
	var id uint64 = 0
	var cid uint64 = 0
	if len(bytes) < 3 {
		return id, cid
	}
	var l int
	l = int(bytes[0])
	var a1 int
	a1 = int(l % 16)
	var a2 int
	a2 = int(l / 16)

	var b1 [8]byte
	var b2 [8]byte

	copy(b1[:], bytes[1:1+a1])
	copy(b2[:], bytes[1+a1:1+a1+a2])

	v1 := binary.LittleEndian.Uint64(b1[:])
	v2 := binary.LittleEndian.Uint64(b2[:])

	return v1, v2
}

func byteLen(v uint64) int {
	if v == 0 {
		return 0
	}
	return (bits.Len64(v) + 7) / 8
}

func SetType(v uint32) []byte {
	var b1 [4]byte
	binary.LittleEndian.PutUint32(b1[:], v)

	ll := 3
	for b1[ll] != 0 && ll != 0 {
		ll--
	}

	out := make([]byte, 1+ll)
	out[0] = b1[0]*4 + byte(ll)
	copy(out[1:], b1[1:1+ll])

	return out
}

func SetId(id, cid uint64) []byte {
	a1 := byteLen(id)
	a2 := byteLen(cid)
	bytes := make([]byte, 1+a1+a2)

	// Store lengths.
	bytes[0] = byte((a2 << 4) | a1)

	var b [8]byte

	// Encode id.
	binary.LittleEndian.PutUint64(b[:], id)
	copy(bytes[1:1+a1], b[:a1])

	// Encode cid.
	binary.LittleEndian.PutUint64(b[:], cid)
	copy(bytes[1+a1:1+a1+a2], b[:a2])

	return bytes
}
