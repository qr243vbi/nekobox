package data

import (
	"github.com/klauspost/compress/zstd"
	"google.golang.org/protobuf/proto"
)

func ProtoToBytes(msg proto.Message) ([]byte, error) {
	raw, err := proto.Marshal(msg)
	if err != nil {
		return nil, err
	}

	encoder, err := zstd.NewWriter(nil)
	if err != nil {
		return nil, err
	}
	defer encoder.Close()

	return encoder.EncodeAll(raw, nil), nil
}

func BytesToProto(data []byte, msg proto.Message) error {
	// Decompress zstd
	decoder, err := zstd.NewReader(nil)
	if err != nil {
		return err
	}
	defer decoder.Close()

	raw, err := decoder.DecodeAll(data, nil)
	if err != nil {
		return err
	}

	// Deserialize protobuf
	return proto.Unmarshal(raw, msg)
}
