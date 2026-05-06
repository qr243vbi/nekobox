//go:build !unix

package control

func ProtectPath(protectPath string) Func {
	return nil
}
