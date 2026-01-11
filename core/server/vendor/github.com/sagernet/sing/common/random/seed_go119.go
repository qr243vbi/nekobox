//go:build !go1.20

package random

import (
	"crypto/rand"
	"encoding/binary"
	mRand "math/rand"
	"sync"

	"github.com/sagernet/sing/common"
)

var initSeedOnce sync.Once

func InitializeSeed() {
	initSeedOnce.Do(initializeSeed)
}

func initializeSeed() {
	var seed int64
	common.Must(binary.Read(rand.Reader, binary.LittleEndian, &seed))
	mRand.Seed(seed)
}
