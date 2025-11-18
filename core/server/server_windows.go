package main

import (
	"Core/gen"
	"Core/internal/boxdns"
	"context"
	"crypto/rand"
	"fmt"
	"io"
	"log"
	"math/big"
	"os"
	"sync"
	"syscall"
	"unsafe"

	"github.com/NullYing/npipe"
	"golang.org/x/sys/windows"
)

const (
	SEE_MASK_NOCLOSEPROCESS = 0x00000040
	SW_HIDE                 = 0
)

type ShellExecuteInfo struct {
	cbSize       uint32
	fMask        uint32
	hwnd         syscall.Handle
	lpVerb       *uint16
	lpFile       *uint16
	lpParameters *uint16
	lpDirectory  *uint16
	nShow        int32
	hInstApp     syscall.Handle
	lpIDList     unsafe.Pointer
	lpClass      *uint16
	hkeyClass    syscall.Handle
	dwHotKey     uint32
	hIcon        syscall.Handle
	hProcess     syscall.Handle
}

func shellExecuteEx(sei *ShellExecuteInfo) error {
	modShell32 := syscall.NewLazyDLL("shell32.dll")
	procShellExecuteExW := modShell32.NewProc("ShellExecuteExW")

	r, _, err := procShellExecuteExW.Call(uintptr(unsafe.Pointer(sei)))
	if r == 0 {
		return err
	}
	return nil
}

func getExitCode(hProcess syscall.Handle) (uint32, error) {
	var exitCode uint32
	kernel32 := syscall.NewLazyDLL("kernel32.dll")
	getExitCodeProcess := kernel32.NewProc("GetExitCodeProcess")

	r, _, err := getExitCodeProcess.Call(uintptr(hProcess), uintptr(unsafe.Pointer(&exitCode)))
	if r == 0 {
		return 0, err
	}
	return exitCode, nil
}

func runShellExec(command string, arguments string) (int, error) {
	verbPtr, _ := syscall.UTF16PtrFromString("runas") // Elevate
	filePtr, _ := syscall.UTF16PtrFromString(command)
	paramsPtr, _ := syscall.UTF16PtrFromString(arguments)

	sei := &ShellExecuteInfo{
		cbSize:       uint32(unsafe.Sizeof(ShellExecuteInfo{})),
		fMask:        SEE_MASK_NOCLOSEPROCESS,
		hwnd:         0,
		lpVerb:       verbPtr,
		lpFile:       filePtr,
		lpParameters: paramsPtr,
		nShow:        SW_HIDE,
	}

	err := shellExecuteEx(sei)
	if err != nil {
		return 1, fmt.Errorf("Failed to start elevated process:", err)
	}

	// Wait for the process to finish
	syscall.WaitForSingleObject(sei.hProcess, syscall.INFINITE)

	// Get exit code
	exitCode, err := getExitCode(sei.hProcess)
	if err != nil {
		return 1, fmt.Errorf("Failed to get exit code:", err)
	}

	return int(exitCode), nil
}

const letters = "abcdefghijklmnopqrstuvwxyz"

func RandString(n int) (string, error) {
	result := make([]byte, n)
	for i := 0; i < n; i++ {
		num, err := rand.Int(rand.Reader, big.NewInt(int64(len(letters))))
		if err != nil {
			return "", err
		}
		result[i] = letters[num.Int64()]
	}
	return string(result), nil
}

func (s *server) SetSystemDNS(in *gen.SetSystemDNSRequest, out *gen.EmptyResp) error {
	err := boxdns.DnsManagerInstance.SetSystemDNS(nil, in.Clear)
	if err != nil {
		return err
	}

	return nil
}

func handlePipe(pipeName string, output io.Writer, wg *sync.WaitGroup) {
	defer wg.Done()

	ln, err := npipe.Listen(pipeName)
	if err != nil {
		log.Fatalf("Error listening on pipe %s: %v\n", pipeName, err)
	}
	defer ln.Close()

	//log.Printf("Waiting for client on pipe: %s\n", pipeName)
	conn, err := ln.Accept()
	if err != nil {
		log.Fatalf("Accept error on %s: %v\n", pipeName, err)
		return
	}
	defer conn.Close()

	//	fmt.Printf("Client connected to %s\n", pipeName)

	_, err = io.Copy(output, conn)
	if err != nil {
		log.Fatalf("Error copying from %s: %v\n", pipeName, err)
	}
}

func runAdmin(_port *int, _debug *bool) (int, error) {
	executablePath, err := os.Executable()
	if err != nil {
		log.Fatalf("Failed to get executable path: %v", err)
	}

	randstr, _ := RandString(6)

	stdout_pipe := `\\.\pipe\nekobox_core_stdout_` + randstr
	stderr_pipe := `\\.\pipe\nekobox_core_stderr_` + randstr
	flag := ""
	if *_debug {
		flag = " \"-debug\""
	}

	var pid int
	pid = os.Getpid()

	//	formattedString := fmt.Sprintf("Start-Process \"%s\" -ArgumentList '-port %d -waitpid %d -redirect-output \"%s\" -redirect-error \"%s\"%s' -WindowStyle Hidden -Verb RunAs -Wait",
	//		 os.Args[0], *_port, pid, stdout_pipe, stderr_pipe, flag)

	formattedString := fmt.Sprintf("-port %d -waitpid %d -redirect-output \"%s\" -redirect-error \"%s\"%s", *_port, pid, stdout_pipe, stderr_pipe, flag)

	var wg sync.WaitGroup

	// Pipe for stdout
	wg.Add(1)
	go handlePipe(stdout_pipe, os.Stdout, &wg)

	// Pipe for stderr
	wg.Add(1)
	go handlePipe(stderr_pipe, os.Stderr, &wg)

	return runShellExec(executablePath, formattedString)
	/*
			cmd := exec.Command("powershell", "-Command", formattedString)
			err := cmd.Run()

			var code int

			if err != nil {
		        // Process exited with error
		        if exitErr, ok := err.(*exec.ExitError); ok {
		            code = exitErr.ExitCode()
		        } else {
					code = -1
				}
		    } else {
		        // Process exited successfully
		        code = 0
		    }

			return code, nil
	*/
}

func isElevated() (bool, error) {
	var token windows.Token
	err := windows.OpenProcessToken(windows.CurrentProcess(), windows.TOKEN_QUERY, &token)
	if err != nil {
		return false, err
	}
	defer token.Close()

	var elevation uint32
	var outLen uint32
	err = windows.GetTokenInformation(
		token,
		windows.TokenElevation,
		(*byte)(unsafe.Pointer(&elevation)),
		uint32(unsafe.Sizeof(elevation)),
		&outLen,
	)
	if err != nil {
		return false, err
	}

	return elevation != 0, nil
}

// WaitForProcessExit waits for a Windows process (by PID) to exit.
func WaitForProcessExit(pid int) error {
	handle, err := windows.OpenProcess(windows.SYNCHRONIZE, false, uint32(pid))
	if err != nil {
		return fmt.Errorf("failed to open process with PID %d: %w", pid, err)
	}
	defer windows.CloseHandle(handle)

	status, err := windows.WaitForSingleObject(handle, windows.INFINITE)
	if err != nil {
		return fmt.Errorf("wait failed: %w", err)
	}

	if status != windows.WAIT_OBJECT_0 {
		return fmt.Errorf("unexpected wait status: %d", status)
	}

	return nil
}

func (s *server) IsPrivileged(ctx context.Context, in *gen.EmptyReq) (*gen.IsPrivilegedResponse, error) {
	elevated, err := isElevated()
	out := new(gen.IsPrivilegedResponse)
	if err != nil {
		out.HasPrivilege = (false)
		return out, err
	}
	out.HasPrivilege = (elevated)
	return out, nil
}

func restartAsAdmin() {
}
