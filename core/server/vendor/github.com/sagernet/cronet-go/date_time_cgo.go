//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import (
	"time"
	"unsafe"
)

func NewDateTime() DateTime {
	return DateTime{uintptr(unsafe.Pointer(C.Cronet_DateTime_Create()))}
}

func (t DateTime) Destroy() {
	C.Cronet_DateTime_Destroy(C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)))
}

// SetValue
// Number of milliseconds since the UNIX epoch.
func (t DateTime) SetValue(value time.Time) {
	C.Cronet_DateTime_value_set(C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)), C.int64_t(value.UnixMilli()))
}

func (t DateTime) Value() time.Time {
	return time.UnixMilli(int64(C.Cronet_DateTime_value_get(C.Cronet_DateTimePtr(unsafe.Pointer(t.ptr)))))
}
