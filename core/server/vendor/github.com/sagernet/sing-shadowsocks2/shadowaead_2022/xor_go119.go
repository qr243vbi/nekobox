//go:build !go1.20

package shadowaead_2022

import _ "unsafe"

//go:linkname xorWords crypto/cipher.xorWords
//go:noescape
func xorWords(dst, a, b []byte)
