package badjson

import (
	"bytes"

	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/json"
)

type JSONArray []any

func (a JSONArray) IsEmpty() bool {
	if len(a) == 0 {
		return true
	}
	return common.All(a, func(it any) bool {
		if valueInterface, valueMaybeEmpty := it.(isEmpty); valueMaybeEmpty && valueInterface.IsEmpty() {
			return true
		}
		return false
	})
}

func (a JSONArray) MarshalJSON() ([]byte, error) {
	return json.Marshal([]any(a))
}

func (a *JSONArray) UnmarshalJSON(content []byte) error {
	decoder := json.NewDecoder(bytes.NewReader(content))
	arrayStart, err := decoder.Token()
	if err != nil {
		return err
	} else if arrayStart != json.Delim('[') {
		return E.New("excepted array start, but got ", arrayStart)
	}
	err = a.decodeJSON(decoder)
	if err != nil {
		return err
	}
	arrayEnd, err := decoder.Token()
	if err != nil {
		return err
	} else if arrayEnd != json.Delim(']') {
		return E.New("excepted array end, but got ", arrayEnd)
	}
	return nil
}

func (a *JSONArray) decodeJSON(decoder *json.Decoder) error {
	for decoder.More() {
		item, err := decodeJSON(decoder)
		if err != nil {
			return err
		}
		*a = append(*a, item)
	}
	return nil
}
