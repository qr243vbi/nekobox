//go:build !linux

package control

func RoutingMark(mark uint32) Func {
	return nil
}
