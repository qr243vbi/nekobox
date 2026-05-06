package replay

/*
import (
	"sync"
	"time"

	"github.com/seiflotfy/cuckoofilter"
)

const defaultCapacity = 100000

func NewCuckoo(interval time.Duration) *CuckooFilter {
	return &CuckooFilter{
		poolA:    cuckoo.NewFilter(defaultCapacity),
		poolB:    cuckoo.NewFilter(defaultCapacity),
		lastSwap: time.Now(),
		interval: interval,
	}
}

type CuckooFilter struct {
	access   sync.Mutex
	poolA    *cuckoo.Filter
	poolB    *cuckoo.Filter
	poolSwap bool
	lastSwap time.Time
	interval time.Duration
}

func (f *CuckooFilter) Check(sum []byte) bool {
	f.access.Lock()
	defer f.access.Unlock()

	now := time.Now()

	elapsed := now.Sub(f.lastSwap)
	if elapsed >= f.interval {
		if f.poolSwap {
			f.poolA.Reset()
		} else {
			f.poolB.Reset()
		}
		f.poolSwap = !f.poolSwap
		f.lastSwap = now
	}

	return f.poolA.InsertUnique(sum) && f.poolB.InsertUnique(sum)
}
*/
