package binary

import (
	"encoding/binary"
	"reflect"
)

func DataSize(t reflect.Value) int {
	return dataSize(t)
}

func EncodeValue(order binary.ByteOrder, buf []byte, v reflect.Value) {
	(&encoder{order: order, buf: buf}).value(v)
}

func DecodeValue(order binary.ByteOrder, buf []byte, v reflect.Value) {
	(&decoder{order: order, buf: buf}).value(v)
}
