package json

import "context"

type ContextMarshaler interface {
	MarshalJSONContext(ctx context.Context) ([]byte, error)
}

type ContextUnmarshaler interface {
	UnmarshalJSONContext(ctx context.Context, content []byte) error
}
