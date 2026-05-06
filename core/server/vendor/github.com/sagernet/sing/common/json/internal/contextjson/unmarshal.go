package json

import "context"

func UnmarshalDisallowUnknownFields(data []byte, v any) error {
	var d decodeState
	d.disallowUnknownFields = true
	err := checkValid(data, &d.scan)
	if err != nil {
		return err
	}
	d.init(data)
	return d.unmarshal(v)
}

func UnmarshalContextDisallowUnknownFields(ctx context.Context, data []byte, v any) error {
	var d decodeState
	d.ctx = ctx
	d.disallowUnknownFields = true
	err := checkValid(data, &d.scan)
	if err != nil {
		return err
	}
	d.init(data)
	return d.unmarshal(v)
}
