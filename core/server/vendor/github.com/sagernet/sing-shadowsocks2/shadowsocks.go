package shadowsocks

import (
	"context"

	C "github.com/sagernet/sing-shadowsocks2/cipher"
	_ "github.com/sagernet/sing-shadowsocks2/shadowaead"
	_ "github.com/sagernet/sing-shadowsocks2/shadowaead_2022"
	_ "github.com/sagernet/sing-shadowsocks2/shadowstream"
)

type (
	Method        = C.Method
	MethodOptions = C.MethodOptions
)

func CreateMethod(ctx context.Context, method string, options MethodOptions) (Method, error) {
	return C.CreateMethod(ctx, method, options)
}
