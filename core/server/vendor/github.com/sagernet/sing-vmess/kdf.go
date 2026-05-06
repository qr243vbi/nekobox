package vmess

import (
	"crypto/hmac"
	"crypto/sha256"
	"hash"
)

func KDF(key []byte, salt string, path ...[]byte) []byte {
	hmacCreator := &hMacCreator{value: []byte(KDFSaltConstVMessAEADKDF)}
	hmacCreator = &hMacCreator{value: []byte(salt), parent: hmacCreator}
	for _, v := range path {
		hmacCreator = &hMacCreator{value: v, parent: hmacCreator}
	}
	hmacf := hmacCreator.Create()
	hmacf.Write(key)
	return hmacf.Sum(nil)
}

type hMacCreator struct {
	parent *hMacCreator
	value  []byte
}

func (h *hMacCreator) Create() hash.Hash {
	if h.parent == nil {
		return hmac.New(sha256.New, h.value)
	}
	return hmac.New(h.parent.Create, h.value)
}
