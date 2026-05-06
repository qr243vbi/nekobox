package badoption

import (
	"context"

	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/json"
)

type Listable[T any] []T

func (l Listable[T]) MarshalJSONContext(ctx context.Context) ([]byte, error) {
	arrayList := []T(l)
	if len(arrayList) == 1 {
		return json.Marshal(arrayList[0])
	}
	return json.MarshalContext(ctx, arrayList)
}

func (l *Listable[T]) UnmarshalJSONContext(ctx context.Context, content []byte) error {
	var singleItem *T
	err := json.UnmarshalContextDisallowUnknownFields(ctx, content, &singleItem)
	if err == nil {
		if singleItem != nil {
			*l = []T{*singleItem}
		}
		return nil
	}
	newErr := json.UnmarshalContextDisallowUnknownFields(ctx, content, (*[]T)(l))
	if newErr == nil {
		return nil
	}
	return E.Errors(err, newErr)
}
