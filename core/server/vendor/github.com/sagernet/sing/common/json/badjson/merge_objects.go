package badjson

import (
	"context"
	"reflect"

	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/json"
	cJSON "github.com/sagernet/sing/common/json/internal/contextjson"
)

func MarshallObjects(objects ...any) ([]byte, error) {
	return MarshallObjectsContext(context.Background(), objects...)
}

func MarshallObjectsContext(ctx context.Context, objects ...any) ([]byte, error) {
	if len(objects) == 1 {
		return json.Marshal(objects[0])
	}
	var content JSONObject
	for _, object := range objects {
		objectMap, err := newJSONObject(ctx, object)
		if err != nil {
			return nil, err
		}
		content.PutAll(objectMap)
	}
	return content.MarshalJSONContext(ctx)
}

func UnmarshallExcluded(inputContent []byte, parentObject any, object any) error {
	return UnmarshallExcludedContext(context.Background(), inputContent, parentObject, object)
}

func UnmarshallExcludedContext(ctx context.Context, inputContent []byte, parentObject any, object any) error {
	var content JSONObject
	err := content.UnmarshalJSONContext(ctx, inputContent)
	if err != nil {
		return err
	}
	for _, key := range cJSON.ObjectKeys(reflect.TypeOf(parentObject)) {
		content.Remove(key)
	}
	if object == nil {
		if content.IsEmpty() {
			return nil
		}
		return E.New("unexpected key: ", content.Keys()[0])
	}
	inputContent, err = content.MarshalJSONContext(ctx)
	if err != nil {
		return err
	}
	return json.UnmarshalContextDisallowUnknownFields(ctx, inputContent, object)
}

func UnmarshallExcludedMulti(inputContent []byte, parentObject any, object any) error {
	return UnmarshallExcludedContextMulti(context.Background(), inputContent, parentObject, object)
}

func UnmarshallExcludedContextMulti(ctx context.Context, inputContent []byte, parentObject any, object any) error {
	var content JSONObject
	err := content.UnmarshalJSONContext(ctx, inputContent)
	if err != nil {
		return err
	}
	parentBinary, err := json.MarshalContext(ctx, parentObject)
	if err != nil {
		return err
	}
	var parentMap map[string]any
	err = json.UnmarshalContext(ctx, parentBinary, &parentMap)
	if err != nil {
		return err
	}
	for key := range parentMap {
		content.Remove(key)
	}
	if object == nil {
		if content.IsEmpty() {
			return nil
		}
		return E.New("unexpected key: ", content.Keys()[0])
	}
	inputContent, err = content.MarshalJSONContext(ctx)
	if err != nil {
		return err
	}
	return json.UnmarshalContextDisallowUnknownFields(ctx, inputContent, object)
}

func newJSONObject(ctx context.Context, object any) (*JSONObject, error) {
	inputContent, err := json.MarshalContext(ctx, object)
	if err != nil {
		return nil, err
	}
	var content JSONObject
	err = content.UnmarshalJSONContext(ctx, inputContent)
	if err != nil {
		return nil, err
	}
	return &content, nil
}
