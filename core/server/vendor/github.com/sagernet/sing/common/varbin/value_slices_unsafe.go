package varbin

import (
	"reflect"
	"unsafe"
)

type myValue struct {
	typ_ *any
	ptr  unsafe.Pointer
}

func slicesValue[T any](value reflect.Value) []T {
	v := (*myValue)(unsafe.Pointer(&value))
	return *(*[]T)(v.ptr)
}

func setSliceValue[T any](value reflect.Value, x []T) {
	v := (*myValue)(unsafe.Pointer(&value))
	*(*[]T)(v.ptr) = x
}

func baseDataSlices(data reflect.Value) any {
	switch data.Type().Elem().Kind() {
	case reflect.Bool:
		return slicesValue[bool](data)
	case reflect.Int8:
		return slicesValue[int8](data)
	case reflect.Uint8:
		return slicesValue[uint8](data)
	case reflect.Int16:
		return slicesValue[int16](data)
	case reflect.Uint16:
		return slicesValue[uint16](data)
	case reflect.Int32:
		return slicesValue[int32](data)
	case reflect.Uint32:
		return slicesValue[uint32](data)
	case reflect.Int64:
		return slicesValue[int64](data)
	case reflect.Uint64:
		return slicesValue[uint64](data)
	case reflect.Float32:
		return slicesValue[float32](data)
	case reflect.Float64:
		return slicesValue[float64](data)
	default:
		return nil
	}
}

func makeBaseDataSlices(data reflect.Value, dataLen int) any {
	switch data.Type().Elem().Kind() {
	case reflect.Bool:
		return make([]bool, dataLen)
	case reflect.Int8:
		return make([]int8, dataLen)
	case reflect.Uint8:
		return make([]uint8, dataLen)
	case reflect.Int16:
		return make([]int16, dataLen)
	case reflect.Uint16:
		return make([]uint16, dataLen)
	case reflect.Int32:
		return make([]int32, dataLen)
	case reflect.Uint32:
		return make([]uint32, dataLen)
	case reflect.Int64:
		return make([]int64, dataLen)
	case reflect.Uint64:
		return make([]uint64, dataLen)
	case reflect.Float32:
		return make([]float32, dataLen)
	case reflect.Float64:
		return make([]float64, dataLen)
	default:
		return nil
	}
}

func setBaseDataSlices(data reflect.Value, rawDataSlices any) {
	switch dataSlices := rawDataSlices.(type) {
	case []bool:
		setSliceValue(data, dataSlices)
	case []int8:
		setSliceValue(data, dataSlices)
	case []uint8:
		setSliceValue(data, dataSlices)
	case []int16:
		setSliceValue(data, dataSlices)
	case []uint16:
		setSliceValue(data, dataSlices)
	case []int32:
		setSliceValue(data, dataSlices)
	case []uint32:
		setSliceValue(data, dataSlices)
	case []int64:
		setSliceValue(data, dataSlices)
	case []uint64:
		setSliceValue(data, dataSlices)
	case []float32:
		setSliceValue(data, dataSlices)
	case []float64:
		setSliceValue(data, dataSlices)
	}
}
