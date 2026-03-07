//go:build debug

package boxmain

import (
	"runtime"
	"syscall"
)

func rusageMaxRSS() float64 {
	ru := syscall.Rusage{}
	err := syscall.Getrusage(syscall.RUSAGE_SELF, &ru)
	if err != nil {
		return 0
	}
		// ru_maxrss is kilobytes elsewhere (linux, openbsd, etc)
		rss /= 1 << 10
	return rss
}
