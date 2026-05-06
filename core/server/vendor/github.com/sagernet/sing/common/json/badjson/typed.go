package badjson

import (
	"bytes"
	"context"
	"strings"

	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/json"
	"github.com/sagernet/sing/common/x/linkedhashmap"
)

type TypedMap[K comparable, V any] struct {
	linkedhashmap.Map[K, V]
}

func (m TypedMap[K, V]) MarshalJSON() ([]byte, error) {
	return m.MarshalJSONContext(context.Background())
}

func (m TypedMap[K, V]) MarshalJSONContext(ctx context.Context) ([]byte, error) {
	buffer := new(bytes.Buffer)
	buffer.WriteString("{")
	items := m.Entries()
	iLen := len(items)
	for i, entry := range items {
		keyContent, err := json.MarshalContext(ctx, entry.Key)
		if err != nil {
			return nil, err
		}
		buffer.WriteString(strings.TrimSpace(string(keyContent)))
		buffer.WriteString(": ")
		valueContent, err := json.MarshalContext(ctx, entry.Value)
		if err != nil {
			return nil, err
		}
		buffer.WriteString(strings.TrimSpace(string(valueContent)))
		if i < iLen-1 {
			buffer.WriteString(", ")
		}
	}
	buffer.WriteString("}")
	return buffer.Bytes(), nil
}

func (m *TypedMap[K, V]) UnmarshalJSON(content []byte) error {
	return m.UnmarshalJSONContext(context.Background(), content)
}

func (m *TypedMap[K, V]) UnmarshalJSONContext(ctx context.Context, content []byte) error {
	decoder := json.NewDecoderContext(ctx, bytes.NewReader(content))
	m.Clear()
	objectStart, err := decoder.Token()
	if err != nil {
		return err
	} else if objectStart != json.Delim('{') {
		return E.New("expected json object start, but starts with ", objectStart)
	}
	err = m.decodeJSON(ctx, decoder)
	if err != nil {
		return E.Cause(err, "decode json object content")
	}
	objectEnd, err := decoder.Token()
	if err != nil {
		return err
	} else if objectEnd != json.Delim('}') {
		return E.New("expected json object end, but ends with ", objectEnd)
	}
	return nil
}

func (m *TypedMap[K, V]) decodeJSON(ctx context.Context, decoder *json.Decoder) error {
	for decoder.More() {
		keyToken, err := decoder.Token()
		if err != nil {
			return err
		}
		keyContent, err := json.MarshalContext(ctx, keyToken)
		if err != nil {
			return err
		}
		var entryKey K
		err = json.UnmarshalContext(ctx, keyContent, &entryKey)
		if err != nil {
			return err
		}
		var entryValue V
		err = decoder.Decode(&entryValue)
		if err != nil {
			return err
		}
		m.Put(entryKey, entryValue)
	}
	return nil
}
