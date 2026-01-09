//go:build !go1.21

package winpowrprof

type myPinner struct{}

func (p *myPinner) Pin(pointer any) {
}

func (p *myPinner) Unpin() {
}
