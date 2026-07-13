package data

import (
	bolt "go.etcd.io/bbolt"
	"google.golang.org/protobuf/proto"
)

type DatabaseType int

const (
	Bolt DatabaseType = iota
	Binary
	Json
)

type Database struct {
	folder string
	db     *bolt.DB
	kind   DatabaseType
}

func (db *Database) getDB() *bolt.DB {
	return db.db
}

func (db *Database) SetKind(t DatabaseType) {
	db.kind = t
}

func (db *Database) GetKind() DatabaseType {
	return db.kind
}

func (db *Database) SaveMessage(msg proto.Message) error {
	var err error = nil
	switch db.kind {
	case Bolt:
		err = SaveMessageBolt(msg, db.getDB())
	case Json:
		err = SaveMessageFilesystem(msg, db.folder, true)
	case Binary:
		err = SaveMessageFilesystem(msg, db.folder, false)
	}
	return err
}
