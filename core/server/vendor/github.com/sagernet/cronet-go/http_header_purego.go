//go:build with_purego

package cronet

import (
	"github.com/sagernet/cronet-go/internal/cronet"
)

func NewHTTPHeader() HTTPHeader {
	return HTTPHeader{cronet.HttpHeaderCreate()}
}

func (h HTTPHeader) Destroy() {
	cronet.HttpHeaderDestroy(h.ptr)
}

func (h HTTPHeader) SetName(name string) {
	cronet.HttpHeaderNameSet(h.ptr, name)
}

func (h HTTPHeader) Name() string {
	return cronet.HttpHeaderNameGet(h.ptr)
}

func (h HTTPHeader) SetValue(value string) {
	cronet.HttpHeaderValueSet(h.ptr, value)
}

func (h HTTPHeader) Value() string {
	return cronet.HttpHeaderValueGet(h.ptr)
}
