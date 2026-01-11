package http

import (
	"bufio"
	"net/http"
	"net/url"
	_ "unsafe" // for linkname
)

//go:linkname ReadRequest net/http.readRequest
func ReadRequest(b *bufio.Reader) (req *http.Request, err error)

//go:linkname URLSetPath net/url.(*URL).setPath
func URLSetPath(u *url.URL, p string) error

//go:linkname ParseBasicAuth net/http.parseBasicAuth
func ParseBasicAuth(auth string) (username, password string, ok bool)
