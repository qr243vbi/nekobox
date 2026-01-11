package badoption

import "net/http"

type HTTPHeader map[string]Listable[string]

func (h HTTPHeader) Build() http.Header {
	header := make(http.Header)
	for name, values := range h {
		for _, value := range values {
			header.Add(name, value)
		}
	}
	return header
}
