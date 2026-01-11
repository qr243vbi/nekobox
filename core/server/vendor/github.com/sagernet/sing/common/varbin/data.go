package varbin

import (
	"errors"
	"io"
	"reflect"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/binary"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
)

func Read(r io.Reader, order binary.ByteOrder, rawData any) error {
	reader := StubReader(r)
	switch data := rawData.(type) {
	case *[]bool:
		return readBase(reader, order, data)
	case *[]int8:
		return readBase(reader, order, data)
	case *[]uint8:
		return readBase(reader, order, data)
	case *[]int16:
		return readBase(reader, order, data)
	case *[]uint16:
		return readBase(reader, order, data)
	case *[]int32:
		return readBase(reader, order, data)
	case *[]uint32:
		return readBase(reader, order, data)
	case *[]int64:
		return readBase(reader, order, data)
	case *[]uint64:
		return readBase(reader, order, data)
	case *[]float32:
		return readBase(reader, order, data)
	case *[]float64:
		return readBase(reader, order, data)
	default:
		if intBaseDataSize(rawData) != 0 {
			return binary.Read(reader, order, rawData)
		}
	}
	return read(reader, order, reflect.Indirect(reflect.ValueOf(rawData)), false)
}

func ReadValue[T any](r io.Reader, order binary.ByteOrder) (T, error) {
	var value T
	err := Read(r, order, &value)
	if err != nil {
		return common.DefaultValue[T](), err
	}
	return value, nil
}

func readBase[T any](r Reader, order binary.ByteOrder, data *[]T) error {
	dataLen, err := binary.ReadUvarint(r)
	if err != nil {
		return E.Cause(err, "slice length")
	}
	if dataLen == 0 {
		*data = nil
		return nil
	}
	dataSlices := make([]T, dataLen)
	err = binary.Read(r, order, dataSlices)
	if err != nil {
		return err
	}
	*data = dataSlices
	return nil
}

func read(r Reader, order binary.ByteOrder, data reflect.Value, isArrayMapValue bool) error {
	switch data.Kind() {
	case reflect.Pointer:
		if !isArrayMapValue {
			pointerValue, err := r.ReadByte()
			if err != nil {
				return err
			}
			if pointerValue == 0 {
				data.SetZero()
				return nil
			}
		}
		if data.IsNil() {
			data.Set(reflect.New(data.Type().Elem()))
		}
		return read(r, order, data.Elem(), false)
	case reflect.String:
		stringLength, err := binary.ReadUvarint(r)
		if err != nil {
			return E.Cause(err, "string length")
		}
		if stringLength == 0 {
			data.SetZero()
		} else {
			stringData := make([]byte, stringLength)
			_, err = io.ReadFull(r, stringData)
			if err != nil {
				return E.Cause(err, "string value")
			}
			data.SetString(string(stringData))
		}
	case reflect.Array:
		arrayLen := data.Len()
		itemSize := data.Type().Elem().Len()
		if itemSize > 0 {
			buf := make([]byte, itemSize*arrayLen)
			_, err := io.ReadFull(r, buf)
			if err != nil {
				return err
			}
			binary.DecodeValue(order, buf, data)
		} else {
			for i := 0; i < arrayLen; i++ {
				err := read(r, order, data.Index(i), true)
				if err != nil {
					return E.Cause(err, "[", i, "]")
				}
			}
		}
	case reflect.Slice:
		var itemLength int
		if !data.IsNil() {
			itemLength = data.Len()
		}
		if itemLength == 0 {
			slicesLength, err := binary.ReadUvarint(r)
			if err != nil {
				return E.Cause(err, "slice length")
			}
			itemLength = int(slicesLength)
		}
		if itemLength > 0 {
			dataSlices := makeBaseDataSlices(data, int(itemLength))
			if dataSlices != nil {
				err := binary.Read(r, order, dataSlices)
				if err != nil {
					return err
				}
				setBaseDataSlices(data, dataSlices)
			} else {
				if !data.IsNil() && data.Len() != itemLength && data.Cap() >= itemLength {
					data.SetLen(itemLength)
				} else {
					data.Set(reflect.MakeSlice(data.Type(), itemLength, itemLength))
				}
				for i := 0; i < itemLength; i++ {
					err := read(r, order, data.Index(i), true)
					if err != nil {
						return E.Cause(err, "[", i, "]")
					}
				}
			}
		}
	case reflect.Map:
		mapLength, err := binary.ReadUvarint(r)
		if err != nil {
			return E.Cause(err, "map length")
		}
		data.Set(reflect.MakeMap(data.Type()))
		for index := 0; index < int(mapLength); index++ {
			key := reflect.New(data.Type().Key()).Elem()
			err = read(r, order, key, false)
			if err != nil {
				return E.Cause(err, "[", index, "].key")
			}
			value := reflect.New(data.Type().Elem()).Elem()
			err = read(r, order, value, true)
			if err != nil {
				return E.Cause(err, "[", index, "].value")
			}
			data.SetMapIndex(key, value)
		}
	case reflect.Struct:
		fieldType := data.Type()
		fieldLen := data.NumField()
		for i := 0; i < fieldLen; i++ {
			field := data.Field(i)
			fieldName := fieldType.Field(i).Name
			if field.CanSet() || fieldName != "_" {
				err := read(r, order, field, false)
				if err != nil {
					return E.Cause(err, fieldName)
				}
			}
		}
	default:
		size := binary.DataSize(data)
		if size < 0 {
			return errors.New("invalid type " + reflect.TypeOf(data).String())
		}
		buf := make([]byte, size)
		_, err := io.ReadFull(r, buf)
		if err != nil {
			return err
		}
		binary.DecodeValue(order, buf, data)
	}
	return nil
}

func Write(w io.Writer, order binary.ByteOrder, rawData any) error {
	if intBaseDataSize(rawData) != 0 {
		return binary.Write(w, order, rawData)
	}
	var (
		writer         Writer
		bufferedWriter *bufio.BufferedWriter
	)
	if bw, ok := w.(Writer); ok {
		writer = bw
	} else {
		bufferedWriter = bufio.NewBufferedWriter(w, buf.NewSize(1024))
		writer = bufferedWriter
	}
	switch data := rawData.(type) {
	case []bool:
		return writeBase(writer, order, data)
	case []int8:
		return writeBase(writer, order, data)
	case []uint8:
		return writeBase(writer, order, data)
	case []int16:
		return writeBase(writer, order, data)
	case []uint16:
		return writeBase(writer, order, data)
	case []int32:
		return writeBase(writer, order, data)
	case []uint32:
		return writeBase(writer, order, data)
	case []int64:
		return writeBase(writer, order, data)
	case []uint64:
		return writeBase(writer, order, data)
	case []float32:
		return writeBase(writer, order, data)
	case []float64:
		return writeBase(writer, order, data)
	}
	err := write(writer, order, reflect.Indirect(reflect.ValueOf(rawData)), false)
	if err != nil {
		return err
	}
	if bufferedWriter != nil {
		err = bufferedWriter.Fallthrough()
		if err != nil {
			return err
		}
	}
	return nil
}

func writeBase[T any](writer Writer, order binary.ByteOrder, data []T) error {
	_, err := WriteUvarint(writer, uint64(len(data)))
	if err != nil {
		return err
	}
	if len(data) > 0 {
		return binary.Write(writer, order, data)
	}
	return nil
}

func write(writer Writer, order binary.ByteOrder, data reflect.Value, isArrayOrMapValue bool) error {
	switch data.Kind() {
	case reflect.Pointer:
		if data.IsNil() {
			if isArrayOrMapValue {
				return E.New("nil array or map value")
			} else {
				err := writer.WriteByte(0)
				if err != nil {
					return err
				}
			}
		} else {
			if !isArrayOrMapValue {
				err := writer.WriteByte(1)
				if err != nil {
					return err
				}
			}
			return write(writer, order, data.Elem(), false)
		}
	case reflect.String:
		stringValue := data.String()
		_, err := WriteUvarint(writer, uint64(len(stringValue)))
		if err != nil {
			return E.Cause(err, "string length")
		}
		if stringValue != "" {
			_, err = writer.Write([]byte(stringValue))
			if err != nil {
				return E.Cause(err, "string value")
			}
		}
	case reflect.Array:
		dataLen := data.Len()
		if dataLen > 0 {
			itemSize := intItemBaseDataSize(data)
			if itemSize > 0 {
				buf := make([]byte, itemSize*dataLen)
				binary.EncodeValue(order, buf, data)
				_, err := writer.Write(buf)
				if err != nil {
					return E.Cause(err, reflect.TypeOf(data).String())
				}
			} else {
				for i := 0; i < dataLen; i++ {
					err := write(writer, order, data.Index(i), true)
					if err != nil {
						return E.Cause(err, "[", i, "]")
					}
				}
			}
		}
	case reflect.Slice:
		dataLen := data.Len()
		_, err := WriteUvarint(writer, uint64(dataLen))
		if err != nil {
			return E.Cause(err, "slice length")
		}
		if dataLen > 0 {
			dataSlices := baseDataSlices(data)
			if dataSlices != nil {
				err = binary.Write(writer, order, dataSlices)
				if err != nil {
					return err
				}
			} else {
				for i := 0; i < dataLen; i++ {
					err = write(writer, order, data.Index(i), true)
					if err != nil {
						return E.Cause(err, "[", i, "]")
					}
				}
			}
		}
	case reflect.Map:
		dataLen := data.Len()
		_, err := WriteUvarint(writer, uint64(dataLen))
		if err != nil {
			return E.Cause(err, "map length")
		}
		if dataLen > 0 {
			for index, key := range data.MapKeys() {
				err = write(writer, order, key, false)
				if err != nil {
					return E.Cause(err, "[", index, "].key")
				}
				err = write(writer, order, data.MapIndex(key), true)
				if err != nil {
					return E.Cause(err, "[", index, "].value")
				}
			}
		}
	case reflect.Struct:
		fieldType := data.Type()
		fieldLen := data.NumField()
		for i := 0; i < fieldLen; i++ {
			field := data.Field(i)
			fieldName := fieldType.Field(i).Name
			if field.CanSet() || fieldName != "_" {
				err := write(writer, order, field, false)
				if err != nil {
					return E.Cause(err, fieldName)
				}
			}
		}
	default:
		size := binary.DataSize(data)
		if size < 0 {
			return errors.New("binary.Write: some values are not fixed-sized in type " + data.Type().String())
		}
		buf := make([]byte, size)
		binary.EncodeValue(order, buf, data)
		_, err := writer.Write(buf)
		if err != nil {
			return E.Cause(err, reflect.TypeOf(data).String())
		}
	}
	return nil
}

func intItemBaseDataSize(data reflect.Value) int {
	itemType := data.Type().Elem()
	switch itemType.Kind() {
	case reflect.Bool,
		reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64,
		reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64,
		reflect.Float32, reflect.Float64, reflect.Complex64, reflect.Complex128:
		return itemType.Len()
	default:
		return -1
	}
}

func intBaseDataSize(data any) int {
	switch data.(type) {
	case bool, int8, uint8,
		*bool, *int8, *uint8:
		return 1
	case int16, uint16, *int16, *uint16:
		return 2
	case int32, uint32, *int32, *uint32:
		return 4
	case int64, uint64, *int64, *uint64:
		return 8
	case float32, *float32:
		return 4
	case float64, *float64:
		return 8
	}
	return 0
}
