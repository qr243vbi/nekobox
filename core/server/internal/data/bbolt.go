package data

import (
	bolt "go.etcd.io/bbolt"
	"google.golang.org/protobuf/proto"
)

func SaveMessageBolt(msg proto.Message, db *bolt.DB) error {
	return db.Update(func(tx *bolt.Tx) error {
		kind, id, err := getKindErr(msg)
		if err == nil {
			b := tx.Bucket(Uint64ToBytes(kind))
			bytes, err := ProtoToBytes(msg)
			if err == nil {
				err = b.Put(Uint64ToBytes(id), bytes)
			}
		}
		return err
	})
}
