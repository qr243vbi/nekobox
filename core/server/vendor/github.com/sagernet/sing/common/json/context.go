//go:build go1.20 && !without_contextjson

package json

import (
	"github.com/sagernet/sing/common/json/internal/contextjson"
)

var (
	Marshal    = json.Marshal
	Unmarshal  = json.Unmarshal
	NewEncoder = json.NewEncoder
	NewDecoder = json.NewDecoder
)

type (
	Encoder     = json.Encoder
	Decoder     = json.Decoder
	Token       = json.Token
	Delim       = json.Delim
	SyntaxError = json.SyntaxError
	RawMessage  = json.RawMessage
)
