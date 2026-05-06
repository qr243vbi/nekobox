package utils

import (
	"math/rand/v2"
)

var (
	h2packCorrectionFactor = 1.2493702770780857
	base62TotalCharsNum    = 62
	base62Chars            = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
)

func H2Base62Pad[T int32 | int64 | int](expectedLen T) string {
	actualLenFloat := float64(expectedLen) * h2packCorrectionFactor
	actualLen := int(actualLenFloat)
	result := make([]byte, actualLen)
	for i := range actualLen {
		result[i] = base62Chars[rand.N(base62TotalCharsNum)]
	}
	return string(result)
}
