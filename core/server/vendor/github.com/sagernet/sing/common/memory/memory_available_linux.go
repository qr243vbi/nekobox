package memory

import (
	"bufio"
	"math"
	"os"
	"strconv"
	"strings"
)

func availableNativeSupported() bool {
	return true
}

func availableNative() uint64 {
	available, ok := cgroupAvailable()
	if ok {
		return available
	}
	return procMemAvailable()
}

func cgroupAvailable() (uint64, bool) {
	max, err := readCgroupUint("/sys/fs/cgroup/memory.max")
	if err == nil && max != math.MaxUint64 {
		current, err := readCgroupUint("/sys/fs/cgroup/memory.current")
		if err == nil && max > current {
			return max - current, true
		}
		return 0, true
	}
	limit, err := readCgroupUint("/sys/fs/cgroup/memory/memory.limit_in_bytes")
	if err == nil && limit != math.MaxUint64 {
		usage, err := readCgroupUint("/sys/fs/cgroup/memory/memory.usage_in_bytes")
		if err == nil && limit > usage {
			return limit - usage, true
		}
		return 0, true
	}
	return 0, false
}

func readCgroupUint(path string) (uint64, error) {
	content, err := os.ReadFile(path)
	if err != nil {
		return 0, err
	}
	text := strings.TrimSpace(string(content))
	if text == "max" {
		return math.MaxUint64, nil
	}
	return strconv.ParseUint(text, 10, 64)
}

func procMemAvailable() uint64 {
	file, err := os.Open("/proc/meminfo")
	if err != nil {
		return 0
	}
	defer file.Close()
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasPrefix(line, "MemAvailable:") {
			fields := strings.Fields(line)
			if len(fields) >= 2 {
				value, err := strconv.ParseUint(fields[1], 10, 64)
				if err == nil {
					return value * 1024
				}
			}
			break
		}
	}
	return 0
}
