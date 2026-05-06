//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import (
	"unsafe"
)

func NewPublicKeyPins() PublicKeyPins {
	return PublicKeyPins{uintptr(unsafe.Pointer(C.Cronet_PublicKeyPins_Create()))}
}

func (p PublicKeyPins) Destroy() {
	C.Cronet_PublicKeyPins_Destroy(C.Cronet_PublicKeyPinsPtr(unsafe.Pointer(p.ptr)))
}

// SetHost set name of the host to which the public keys should be pinned. A host that
// consists only of digits and the dot character is treated as invalid.
func (p PublicKeyPins) SetHost(host string) {
	cHost := C.CString(host)
	C.Cronet_PublicKeyPins_host_set(C.Cronet_PublicKeyPinsPtr(unsafe.Pointer(p.ptr)), cHost)
	C.free(unsafe.Pointer(cHost))
}

func (p PublicKeyPins) Host() string {
	return C.GoString(C.Cronet_PublicKeyPins_host_get(C.Cronet_PublicKeyPinsPtr(unsafe.Pointer(p.ptr))))
}

// AddPinnedSHA256 add pins. each pin is the SHA-256 cryptographic
// hash (in the form of "sha256/<base64-hash-value>") of the DER-encoded ASN.1
// representation of the Subject Public Key Info (SPKI) of the host's X.509 certificate.
// Although, the method does not mandate the presence of the backup pin
// that can be used if the control of the primary private key has been
// lost, it is highly recommended to supply one.
func (p PublicKeyPins) AddPinnedSHA256(hash string) {
	cHash := C.CString(hash)
	C.Cronet_PublicKeyPins_pins_sha256_add(C.Cronet_PublicKeyPinsPtr(unsafe.Pointer(p.ptr)), cHash)
	C.free(unsafe.Pointer(cHash))
}

func (p PublicKeyPins) PinnedSHA256Size() int {
	return int(C.Cronet_PublicKeyPins_pins_sha256_size(C.Cronet_PublicKeyPinsPtr(unsafe.Pointer(p.ptr))))
}

func (p PublicKeyPins) PinnedSHA256At(index int) string {
	return C.GoString(C.Cronet_PublicKeyPins_pins_sha256_at(C.Cronet_PublicKeyPinsPtr(unsafe.Pointer(p.ptr)), C.uint32_t(index)))
}

func (p PublicKeyPins) ClearPinnedSHA256() {
	C.Cronet_PublicKeyPins_pins_sha256_clear(C.Cronet_PublicKeyPinsPtr(unsafe.Pointer(p.ptr)))
}

// SetIncludeSubdomains set whether the pinning policy should be applied to subdomains of |host|.
func (p PublicKeyPins) SetIncludeSubdomains(includeSubdomains bool) {
	C.Cronet_PublicKeyPins_include_subdomains_set(C.Cronet_PublicKeyPinsPtr(unsafe.Pointer(p.ptr)), C.bool(includeSubdomains))
}

func (p PublicKeyPins) IncludeSubdomains() bool {
	return bool(C.Cronet_PublicKeyPins_include_subdomains_get(C.Cronet_PublicKeyPinsPtr(unsafe.Pointer(p.ptr))))
}

// SetExpirationDate set the expiration date for the pins in milliseconds since epoch (as in java.util.Date).
func (p PublicKeyPins) SetExpirationDate(date int64) {
	C.Cronet_PublicKeyPins_expiration_date_set(C.Cronet_PublicKeyPinsPtr(unsafe.Pointer(p.ptr)), C.int64_t(date))
}

func (p PublicKeyPins) ExpirationDate() int64 {
	return int64(C.Cronet_PublicKeyPins_expiration_date_get(C.Cronet_PublicKeyPinsPtr(unsafe.Pointer(p.ptr))))
}
