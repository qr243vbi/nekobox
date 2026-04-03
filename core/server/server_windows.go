package main

import (
	"context"
	"crypto/rand"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"math/big"
	"nekobox_core/gen"
	"nekobox_core/internal"
	"nekobox_core/internal/boxdns"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"unsafe"

	"time"

	"github.com/NullYing/npipe"
	"golang.org/x/sys/windows"

	"github.com/Microsoft/go-winio"
	"github.com/qr243vbi/cmdescape"
)

func runServerHandler(pipeName string) {
	cfg := &winio.PipeConfig{
		SecurityDescriptor: "D:P(A;;GA;;;WD)", // allow everyone
		MessageMode:        false,             // byte mode
		InputBufferSize:    4096,
		OutputBufferSize:   4096,
	}

	fmt.Println("Starting named pipe server...")

	listener, err := winio.ListenPipe(pipeName, cfg)
	if err != nil {
		log.Fatalf("ListenPipe failed: %v", err)
	}
	defer listener.Close()

	fmt.Println("Server is running and waiting for clients...")

	for {
		conn, err := listener.Accept()
		if err != nil {
			log.Printf("Accept error: %v", err)
			time.Sleep(time.Second)
			continue
		}

		fmt.Println("Client connected")

		// Handle each client in a goroutine
		go handleClient(conn)
	}
}

func handleClient(conn io.ReadWriteCloser) {
	defer conn.Close()

	buf := make([]byte, 1024)

	for {
		n, err := conn.Read(buf)
		if err != nil {
			if err != io.EOF {
				log.Printf("Read error: %v", err)
			}
			return
		}

		msg := string(buf[:n])
		fmt.Println("Received:", msg)

		// Echo back or process message
		reply := "Server received: " + msg
		_, err = conn.Write([]byte(reply))
		if err != nil {
			log.Printf("Write error: %v", err)
			return
		}
	}
}

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

type taskScheduleConfig struct {
	exec  string `json:"exec"`
	task  string `json:"task"`
	stdin string `json:"stdin"`
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

func checkTaskScheduler() error {
	path, _ := os.Executable()
	path = filepath.Clean(path)
	execpath := path
	path = path + ".elevated_launcher"
	var err error
	if _, err := os.Stat(path); err == nil {
		var cfg taskScheduleConfig
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		if err := json.Unmarshal(data, &cfg); err != nil {
			return err
		}
		if execpath != cfg.exec {
			return fmt.Errorf("file paths are not same %s: %v", execpath, cfg.exec)
		}
		var wg sync.WaitGroup
		randstr, _ := RandString(6)

		stdout_pipe := `\\.\pipe\nekobox_core_stdout_` + randstr
		stderr_pipe := `\\.\pipe\nekobox_core_stderr_` + randstr

		// Pipe for stdout
		wg.Add(1)
		go handlePipe(stdout_pipe, os.Stdout, &wg)

		// Pipe for stderr
		wg.Add(1)
		go handlePipe(stderr_pipe, os.Stderr, &wg)

		var pid int
		pid = os.Getpid()

		other_args := os.Args[1:]
		other_args = append(other_args, "-redirect-output", stdout_pipe, "-redirect-error", stderr_pipe, "-waitpid", strconv.Itoa(pid))
		formattedString := cmdescape.QuoteCommand(other_args)

		if askRun(formattedString, cfg.stdin, cfg.task) {
			wg.Wait()
			os.Exit(0)
		} else {
			os.Exit(1)
		}

	}
	return err
}

func askRun(args, stdin, task string) bool {
	return false
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
		return 1, fmt.Errorf("Failed to start elevated process: %s", err.Error())
	}

	// Wait for the process to finish
	syscall.WaitForSingleObject(sei.hProcess, syscall.INFINITE)

	// Get exit code
	exitCode, err := getExitCode(sei.hProcess)
	if err != nil {
		return 1, fmt.Errorf("Failed to get exit code: %s", err.Error())
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

func (s *server) SetSystemDNS(ctx context.Context, in *gen.SetSystemDNSRequest) (*gen.EmptyResp, error) {
	err := boxdns.DnsManagerInstance.SetSystemDNS(nil, in.Clear)
	out := new(gen.EmptyResp)
	if err != nil {
		return out, err
	}

	return out, nil
}

func runAdmin() (int, error) {
	executablePath, err := os.Executable()
	if err != nil {
		log.Fatalf("Failed to get executable path: %v", err)
	}

	randstr, _ := RandString(6)

	stdout_pipe := `\\.\pipe\nekobox_core_stdout_` + randstr
	stderr_pipe := `\\.\pipe\nekobox_core_stderr_` + randstr

	var pid int
	pid = os.Getpid()

	other_args := os.Args[1:]
	other_args = append(other_args, "-redirect-output", stdout_pipe, "-redirect-error", stderr_pipe, "-waitpid", strconv.Itoa(pid), "-ruleset-cache-directory", internal.GetRulesetCachedir())
	formattedString := cmdescape.QuoteCommand(other_args)

	var wg sync.WaitGroup

	// Pipe for stdout
	wg.Add(1)
	go handlePipe(stdout_pipe, os.Stdout, &wg)

	// Pipe for stderr
	wg.Add(1)
	go handlePipe(stderr_pipe, os.Stderr, &wg)

	ret, err := runShellExec(executablePath, formattedString)
	wg.Wait()

	return ret, err
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

func restartAsAdmin(save bool) {
}

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

func KillPid(pid uint32) {
	p, err := os.FindProcess(int(pid))
	if err != nil {
		fmt.Printf("Killing %d process error: %s\n", pid, err.Error())
	}
	err = p.Kill()
	if err != nil {
		fmt.Printf("Killing %d process error: %s\n", pid, err.Error())
	}

	fmt.Printf("Killing %d process success\n", pid)
}

func KillProcesses(prefix string, tags map[uint32]bool) {
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

		path = strings.ToLower(filepath.Clean(path))

		if !tags[pid] && strings.HasPrefix(path, prefix) {
			fmt.Printf("Process %s => %d \n", path, pid)
			KillPid(pid)
		}
	}
}

var (
	DownloadDirNames []string = []string{"downloads", "download"}
)

func getDownloadDir() string {
	var downloadDir string = ""

	homeDir, err := os.UserHomeDir()
	if err != nil {
		log.Fatal(err)
	}

	for _, ddn := range DownloadDirNames {
		var dir = filepath.Join(homeDir, ddn)

		if _, err := os.Stat(dir); os.IsNotExist(err) {
		} else {
			downloadDir = dir
			break
		}
	}
	return downloadDir
}

func DownloadWithProgress(url, outPath string) error {

	fmt.Printf("\nDownload from url %s to file %s\n", url, outPath)
	resp, err := http.Get(url)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	out, err := os.Create(outPath)
	if err != nil {
		return err
	}
	defer out.Close()

	_, err = io.Copy(out, resp.Body)
	if err != nil {
		return err
	}

	fmt.Println("\nDownload complete")
	return nil
}

func fileExists(path string) bool {
	info, err := os.Stat(path)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return false
		}
		return false
	}
	return !info.IsDir()
}

func wsainit() {
	var data windows.WSAData

	err := windows.WSAStartup(uint32(0x202), &data)
	defer windows.WSACleanup()
	if err != nil {
		panic(err)
	}
}

func InstallVcRedist() {
	var download string = getDownloadDir()
	var VCRedistDownload string = ""
	var VCRedistFile string = ""
	if download == "" {
	} else {
		if runtime.GOARCH == "amd64" {
			VCRedistDownload = "https://aka.ms/vc14/vc_redist.x64.exe"
			VCRedistFile = "vc14_redist.x64.exe"
		} else if runtime.GOARCH == "arm64" {
			VCRedistDownload = "https://aka.ms/vc14/vc_redist.arm64.exe"
			VCRedistFile = "vc14_redist.arm64.exe"
		} else if runtime.GOARCH == "386" {
			VCRedistDownload = "https://aka.ms/vc14/vc_redist.x86.exe"
			VCRedistFile = "vc14_redist.x86.exe"
		}
		VCRedistFile = filepath.Join(getDownloadDir(), VCRedistFile)
		if !fileExists(VCRedistFile) {
			var err error = DownloadWithProgress(VCRedistDownload, VCRedistFile)
			if err != nil {
				fmt.Printf("Download failed: %s", err.Error())
				os.Exit(1)
			}
		}
		cmd := exec.Command(VCRedistFile)
		cmd.Run()
	}
}

func mustAtoi(s string) uint32 {
	i, err := strconv.Atoi(s)
	if err != nil {
		return 0 // or panic / log
	}
	return uint32(i)
}

func InstallerMode() {

	kill_processes := flag.String("kill-processes", "", "Kill processes from directory")
	install_vcpkg := flag.Bool("vcredist-install", false, "Install VcRedist")

	var tags []uint32

	flag.Func("ignore-pid", "", func(value string) error {
		tags = append(tags, mustAtoi(value))
		return nil
	})

	flag.CommandLine.Parse(os.Args[2:])

	tagmap := make(map[uint32]bool)

	var curpid uint32 = uint32(os.Getpid())
	//var ppid uint32 = uint32(os.Getppid())

	fmt.Printf("Current PID %d\n", curpid)
	//	fmt.Printf("Parent PID %d\n", ppid)

	for _, i := range tags {
		tagmap[i] = true
		fmt.Printf("Ignore PID %d\n", i)
	}

	tagmap[curpid] = true
	//	tagmap[ppid] = true

	if *install_vcpkg {
		InstallVcRedist()
	}

	if *kill_processes != "" {
		KillProcesses(*kill_processes, tagmap)
	}

	os.Exit(0)
}
