//go:build !linux

package control

func BindAddressNoPort() Func {
	return nil
}
