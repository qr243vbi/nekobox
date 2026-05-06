package byteformats

import (
	"fmt"
	"math"
)

var (
	unitNames   = []string{"B", "kB", "MB", "GB", "TB", "PB", "EB"}
	iUnitNames  = []string{"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"}
	kUnitNames  = []string{"kB", "MB", "GB", "TB", "PB", "EB"}
	kiUnitNames = []string{"KiB", "MiB", "GiB", "TiB", "PiB", "EiB"}
)

func formatBytes(s uint64, base float64, sizes []string) string {
	if s < 10 {
		return fmt.Sprintf("%d B", s)
	}
	e := math.Floor(logn(float64(s), base))
	suffix := sizes[int(e)]
	val := math.Floor(float64(s)/math.Pow(base, e)*10+0.5) / 10
	f := "%.0f %s"
	if val < 10 {
		f = "%.1f %s"
	}

	return fmt.Sprintf(f, val, suffix)
}

func formatKBytes(s uint64, base float64, sizes []string) string {
	if s == 0 {
		return fmt.Sprintf("0 %s", sizes[0])
	}
	e := math.Floor(logn(float64(s), base))
	if e < 1 {
		e = 1
	}
	suffix := sizes[int(e)-1]
	val := math.Floor(float64(s)/math.Pow(base, e)*10+0.5) / 10
	f := "%.0f %s"
	if val < 10 {
		f = "%.1f %s"
	}

	return fmt.Sprintf(f, val, suffix)
}

func logn(n, b float64) float64 {
	return math.Log(n) / math.Log(b)
}

func FormatBytes(s uint64) string {
	return formatBytes(s, 1000, unitNames)
}

func FormatMemoryBytes(s uint64) string {
	return formatBytes(s, 1024, unitNames)
}

func FormatIBytes(s uint64) string {
	return formatBytes(s, 1024, iUnitNames)
}

func FormatKBytes(s uint64) string {
	return formatKBytes(s, 1000, kUnitNames)
}

func FormatMemoryKBytes(s uint64) string {
	return formatKBytes(s, 1024, kUnitNames)
}

func FormatKIBytes(s uint64) string {
	return formatKBytes(s, 1024, kiUnitNames)
}
