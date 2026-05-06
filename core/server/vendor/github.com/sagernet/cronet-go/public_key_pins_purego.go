//go:build with_purego

package cronet

import (
	"github.com/sagernet/cronet-go/internal/cronet"
)

func NewPublicKeyPins() PublicKeyPins {
	return PublicKeyPins{cronet.PublicKeyPinsCreate()}
}

func (p PublicKeyPins) Destroy() {
	cronet.PublicKeyPinsDestroy(p.ptr)
}

func (p PublicKeyPins) SetHost(host string) {
	cronet.PublicKeyPinsHostSet(p.ptr, host)
}

func (p PublicKeyPins) Host() string {
	return cronet.PublicKeyPinsHostGet(p.ptr)
}

func (p PublicKeyPins) AddPinSHA256(pin string) {
	cronet.PublicKeyPinsPinsSha256Add(p.ptr, pin)
}

func (p PublicKeyPins) PinSHA256Size() int {
	return int(cronet.PublicKeyPinsPinsSha256Size(p.ptr))
}

func (p PublicKeyPins) PinSHA256At(index int) string {
	return cronet.PublicKeyPinsPinsSha256At(p.ptr, uint32(index))
}

func (p PublicKeyPins) SetIncludeSubdomains(include bool) {
	cronet.PublicKeyPinsIncludeSubdomainsSet(p.ptr, include)
}

func (p PublicKeyPins) IncludeSubdomains() bool {
	return cronet.PublicKeyPinsIncludeSubdomainsGet(p.ptr)
}

func (p PublicKeyPins) SetExpirationDate(date int64) {
	cronet.PublicKeyPinsExpirationDateSet(p.ptr, date)
}

func (p PublicKeyPins) ExpirationDate() int64 {
	return cronet.PublicKeyPinsExpirationDateGet(p.ptr)
}
