package legacykey

import (
	"crypto/md5"
	"crypto/sha1"
	"io"

	"github.com/sagernet/sing/common"

	"golang.org/x/crypto/hkdf"
)

func Key(password []byte, keySize int) []byte {
	var b, prev []byte
	h := md5.New()
	for len(b) < keySize {
		h.Write(prev)
		h.Write(password)
		b = h.Sum(b)
		prev = b[len(b)-h.Size():]
		h.Reset()
	}
	return b[:keySize]
}

func Kdf(key, iv, out []byte) {
	kdf := hkdf.New(sha1.New, key, iv, []byte("ss-subkey"))
	common.Must1(io.ReadFull(kdf, out))
}
