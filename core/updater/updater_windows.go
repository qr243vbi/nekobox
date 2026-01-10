package main

import (
	"path/filepath"
	"strings"

	"golang.org/x/sys/windows"
)

func getProcessPath(pid uint32) (string, error) {
	h, err := windows.OpenProcess(
		windows.PROCESS_QUERY_LIMITED_INFORMATION,
		false,
		pid,
	)
	if err != nil {
		return "", err
	}
	defer windows.CloseHandle(h)

	buf := make([]uint16, windows.MAX_PATH)
	size := uint32(len(buf))

	err = windows.QueryFullProcessImageName(
		h,
		0,
		&buf[0],
		&size,
	)
	if err != nil {
		return "", err
	}

	return windows.UTF16ToString(buf[:size]), nil
}

func KillProcesses(prefix string) {
	prefix = strings.ToLower(filepath.Clean(prefix))

	// Enumerate all PIDs
	var pids [1024]uint32
	var bytesReturned uint32

	err := windows.EnumProcesses(pids[:], &bytesReturned)
	if err != nil {
		panic(err)
	}

	count := bytesReturned / 4

	for i := uint32(0); i < count; i++ {
		pid := pids[i]
		if pid == 0 {
			continue
		}

		path, err := getProcessPath(pid)
		if err != nil || path == "" {
			continue
		}

		if strings.HasPrefix(strings.ToLower(filepath.Clean(path)), prefix) {
			//fmt.Printf("PID %d → %s\n", pid, path)
		}
	}
}
