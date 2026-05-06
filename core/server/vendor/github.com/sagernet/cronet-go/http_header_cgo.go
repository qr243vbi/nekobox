//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"
import "unsafe"

func NewHTTPHeader() HTTPHeader {
	return HTTPHeader{uintptr(unsafe.Pointer(C.Cronet_HttpHeader_Create()))}
}

func (h HTTPHeader) Destroy() {
	C.Cronet_HttpHeader_Destroy(C.Cronet_HttpHeaderPtr(unsafe.Pointer(h.ptr)))
}

// SetName sets header name
func (h HTTPHeader) SetName(name string) {
	cName := C.CString(name)
	C.Cronet_HttpHeader_name_set(C.Cronet_HttpHeaderPtr(unsafe.Pointer(h.ptr)), cName)
	C.free(unsafe.Pointer(cName))
}

func (h HTTPHeader) Name() string {
	return C.GoString(C.Cronet_HttpHeader_name_get(C.Cronet_HttpHeaderPtr(unsafe.Pointer(h.ptr))))
}

// SetValue sts header value
func (h HTTPHeader) SetValue(value string) {
	cValue := C.CString(value)
	C.Cronet_HttpHeader_value_set(C.Cronet_HttpHeaderPtr(unsafe.Pointer(h.ptr)), cValue)
	C.free(unsafe.Pointer(cValue))
}

func (h HTTPHeader) Value() string {
	return C.GoString(C.Cronet_HttpHeader_value_get(C.Cronet_HttpHeaderPtr(unsafe.Pointer(h.ptr))))
}
