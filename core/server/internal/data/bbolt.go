package data

import (
	"errors"

	bolt "go.etcd.io/bbolt"
	"google.golang.org/protobuf/proto"
)

func SaveMessage(msg proto.Message, kind uint64, id uint64, db *bolt.DB) error {
	return db.Update(func(tx *bolt.Tx) error {
		if id == 0 {
			return errors.New("Id is 0")
		}
		if kind == 0 {
			return errors.New("Kind is 0")
		}
		b := tx.Bucket(Uint64ToBytes(kind))
		bytes, err := ProtoToBytes(msg)
		if err == nil {
			err = b.Put(Uint64ToBytes(id), bytes)
		}
		return err
	})
}
