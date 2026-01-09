package json

import (
	"context"

	"github.com/sagernet/sing/common/json/internal/contextjson"
)

var (
	MarshalContext                        = json.MarshalContext
	UnmarshalContext                      = json.UnmarshalContext
	NewEncoderContext                     = json.NewEncoderContext
	NewDecoderContext                     = json.NewDecoderContext
	UnmarshalContextDisallowUnknownFields = json.UnmarshalContextDisallowUnknownFields
)

type ContextMarshaler interface {
	MarshalJSONContext(ctx context.Context) ([]byte, error)
}

type ContextUnmarshaler interface {
	UnmarshalJSONContext(ctx context.Context, content []byte) error
}
