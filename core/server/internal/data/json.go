package data

import (
	"google.golang.org/protobuf/encoding/protojson"
	"google.golang.org/protobuf/proto"
)

func ProtoToJSON(msg proto.Message) (string, error) {
	jsonBytes, err := protojson.Marshal(msg)
	if err != nil {
		return "", err
	}

	return string(jsonBytes), nil
}

func JSONToProto(data []byte, msg proto.Message) error {
	return protojson.Unmarshal(data, msg)
}
