package bufio

//go:generate go run golang.org/x/sys/windows/mkwinsyscall -output zsyscall_windows.go syscall_windows.go

//sys recv(s windows.Handle, buf []byte, flags int32) (n int32, err error) [failretval == -1] = ws2_32.recv
