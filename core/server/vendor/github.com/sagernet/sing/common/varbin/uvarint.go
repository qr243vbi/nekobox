package varbin

import "io"

func WriteUvarint(writer io.ByteWriter, value uint64) (int, error) {
	var writeN int
	for value >= 0x80 {
		err := writer.WriteByte(byte(value) | 0x80)
		if err != nil {
			return writeN, err
		}
		value >>= 7
		writeN++
	}
	err := writer.WriteByte(byte(value))
	if err != nil {
		return writeN, err
	}
	return writeN + 1, nil
}

func UvarintLen(x uint64) int {
	switch {
	case x < 1<<(7*1):
		return 1
	case x < 1<<(7*2):
		return 2
	case x < 1<<(7*3):
		return 3
	case x < 1<<(7*4):
		return 4
	case x < 1<<(7*5):
		return 5
	case x < 1<<(7*6):
		return 6
	case x < 1<<(7*7):
		return 7
	default:
		return 8
	}
}
