package data

import (
	"os"
	"strconv"

	"google.golang.org/protobuf/proto"
)

func SaveMessageFilesystem(msg proto.Message, folder string, json bool) error {
	_, id, err := getKindErr(msg)
	if err == nil {
		b := folder + "/" + getName(msg)
		if id != 0 {
			b = b + "/" + strconv.Itoa(int(id))
		}
		b = b + ".cfg"
		var bytes []byte
		if json {
			var str string
			str, err = ProtoToJSON(msg)
			bytes = []byte(str)
		} else {
			bytes, err = ProtoToBytes(msg)
		}
		if err == nil {
			err = os.WriteFile(b, bytes, 0644)
		}
	}
	return err
}
