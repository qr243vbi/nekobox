//go:build go1.20

package shadowaead_2022

import "crypto/subtle"

var xorWords = subtle.XORBytes
