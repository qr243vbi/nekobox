package buf

import "github.com/sagernet/sing/common"

func LenMulti(buffers []*Buffer) int {
	var n int
	for _, buffer := range buffers {
		n += buffer.Len()
	}
	return n
}

func ToSliceMulti(buffers []*Buffer) [][]byte {
	return common.Map(buffers, func(it *Buffer) []byte {
		return it.Bytes()
	})
}

func CopyMulti(toBuffer []byte, buffers []*Buffer) int {
	var n int
	for _, buffer := range buffers {
		n += copy(toBuffer[n:], buffer.Bytes())
	}
	return n
}

func ReleaseMulti(buffers []*Buffer) {
	for _, buffer := range buffers {
		buffer.Release()
	}
}
