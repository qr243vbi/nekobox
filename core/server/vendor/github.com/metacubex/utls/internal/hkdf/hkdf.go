package hkdf

import (
	"crypto/hmac"
	"hash"
)

func Extract[H hash.Hash](h func() H, secret, salt []byte) []byte {
	if salt == nil {
		salt = make([]byte, h().Size())
	}
	extractor := hmac.New(func() hash.Hash { return h() }, salt)
	extractor.Write(secret)

	return extractor.Sum(nil)
}

func Expand[H hash.Hash](h func() H, pseudorandomKey []byte, info string, keyLen int) []byte {
	out := make([]byte, 0, keyLen)
	expander := hmac.New(func() hash.Hash { return h() }, pseudorandomKey)
	var counter uint8
	var buf []byte

	for len(out) < keyLen {
		counter++
		if counter == 0 {
			panic("hkdf: counter overflow")
		}
		if counter > 1 {
			expander.Reset()
		}
		expander.Write(buf)
		expander.Write([]byte(info))
		expander.Write([]byte{counter})
		buf = expander.Sum(buf[:0])
		remain := keyLen - len(out)
		if len(buf) < remain {
			remain = len(buf)
		}
		out = append(out, buf[:remain]...)
	}

	return out
}

func Key[H hash.Hash](h func() H, secret, salt []byte, info string, keyLen int) []byte {
	prk := Extract(h, secret, salt)
	return Expand(h, prk, info, keyLen)
}
