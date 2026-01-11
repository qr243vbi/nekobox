package byteformats

import (
	"fmt"
	"math"
)

var (
	unitNames  = []string{"B", "kB", "MB", "GB", "TB", "PB", "EB"}
	iUnitNames = []string{"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"}
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
