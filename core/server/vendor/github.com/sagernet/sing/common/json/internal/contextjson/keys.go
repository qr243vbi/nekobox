package json

import (
	"reflect"

	"github.com/sagernet/sing/common"
)

func ObjectKeys(object reflect.Type) []string {
	switch object.Kind() {
	case reflect.Pointer:
		return ObjectKeys(object.Elem())
	case reflect.Struct:
	default:
		panic("invalid non-struct input")
	}
	return common.Map(cachedTypeFields(object).list, func(field field) string {
		return field.name
	})
}
