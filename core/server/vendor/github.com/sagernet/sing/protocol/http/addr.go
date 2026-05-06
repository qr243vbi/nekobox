package http

import (
	"net/http"
	"strings"

	M "github.com/sagernet/sing/common/metadata"
)

func SourceAddress(request *http.Request) M.Socksaddr {
	address := M.ParseSocksaddr(request.RemoteAddr)
	forwardFrom := request.Header.Get("X-Forwarded-For")
	if forwardFrom != "" {
		for _, from := range strings.Split(forwardFrom, ",") {
			originAddr := M.ParseAddr(from)
			if originAddr.IsValid() {
				address.Addr = originAddr
				break
			}
		}
	}
	return address.Unwrap()
}
