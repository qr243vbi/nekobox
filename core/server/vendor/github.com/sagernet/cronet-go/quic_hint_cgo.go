//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import (
	"unsafe"
)

func NewQuicHint() QuicHint {
	return QuicHint{uintptr(unsafe.Pointer(C.Cronet_QuicHint_Create()))}
}

func (h QuicHint) Destroy() {
	C.Cronet_QuicHint_Destroy(C.Cronet_QuicHintPtr(unsafe.Pointer(h.ptr)))
}

// SetHost set name of the host that supports QUIC.
func (h QuicHint) SetHost(host string) {
	cHost := C.CString(host)
	C.Cronet_QuicHint_host_set(C.Cronet_QuicHintPtr(unsafe.Pointer(h.ptr)), cHost)
	C.free(unsafe.Pointer(cHost))
}

func (h QuicHint) Host() string {
	return C.GoString(C.Cronet_QuicHint_host_get(C.Cronet_QuicHintPtr(unsafe.Pointer(h.ptr))))
}

// SetPort set port of the server that supports QUIC.
func (h QuicHint) SetPort(port int32) {
	C.Cronet_QuicHint_port_set(C.Cronet_QuicHintPtr(unsafe.Pointer(h.ptr)), C.int32_t(port))
}

func (h QuicHint) Port() int32 {
	return int32(C.Cronet_QuicHint_port_get(C.Cronet_QuicHintPtr(unsafe.Pointer(h.ptr))))
}

// SetAlternatePort set alternate port to use for QUIC
func (h QuicHint) SetAlternatePort(port int32) {
	C.Cronet_QuicHint_alternate_port_set(C.Cronet_QuicHintPtr(unsafe.Pointer(h.ptr)), C.int32_t(port))
}

func (h QuicHint) AlternatePort() int32 {
	return int32(C.Cronet_QuicHint_alternate_port_get(C.Cronet_QuicHintPtr(unsafe.Pointer(h.ptr))))
}
