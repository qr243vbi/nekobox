package main

import (
	"Core/gen"
	"Core/internal/boxmain"
	"context"
	"flag"
	"fmt"
	"log"
	"net"
	"net/http"
	"strconv"
	"os"
	"runtime"
	runtimeDebug "runtime/debug"
	"time"
	_ "Core/internal/distro/all"
	C "github.com/sagernet/sing-box/constant"
	_ "net/http/pprof"
)

func RunCore(_port * int, _debug * bool) {
	debug = *_debug
	boxmain.DisableColor()

	go func() {
		log.Println(http.ListenAndServe("127.0.0.1:54862", nil))
	}()

	// RPC
	go func() {
		for {
			time.Sleep(100 * time.Millisecond)
			conn, err := net.Dial("tcp", "127.0.0.1:"+strconv.Itoa(*_port))
			if err == nil {
				conn.Close()
				fmt.Printf("Core listening at %v\n", "127.0.0.1:"+strconv.Itoa(*_port))
				return
			}
		}
	}()
	err := gen.ListenAndServeLibcoreService("tcp", "127.0.0.1:"+strconv.Itoa(*_port), new(server))
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}
}

func main() {
	var _admin *bool;
	var _waitpid *int;
	_port := flag.Int("port", 19810, "")
	_debug := flag.Bool("debug", false, "")
	
	if runtime.GOOS == "windows" || runtime.GOOS == "linux"{
		_admin = flag.Bool("admin", false, "Run in admin mode")
	}

	_waitpid = flag.Int("waitpid", 0, "After pid finished, force quit")
	
	redirectOutput := flag.String("redirect-output", "", "Path to redirect stdout (e.g. named pipe or file)")
	redirectError := flag.String("redirect-error", "", "Path to redirect stderr (e.g. named pipe or file)")

	flag.CommandLine.Parse(os.Args[1:])
    
	if runtime.GOOS == "linux" {
		if (*_admin){
			restartAsAdmin();
		}
	}

	// Redirect stderr and logs if flag is provided
	if *redirectError != "" {
		errFile, err := os.OpenFile(*redirectError, os.O_WRONLY, 0)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Failed to open error redirect target: %v\n", err)
			os.Exit(1)
		}
		defer errFile.Close()
		os.Stderr = errFile
		log.SetOutput(errFile)
	}
	
	// Redirect stdout if flag is provided
	if *redirectOutput != "" {
		outFile, err := os.OpenFile(*redirectOutput, os.O_WRONLY, 0)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Failed to open output redirect target: %v\n", err)
			os.Exit(1)
		}
		defer outFile.Close()
		os.Stdout = outFile
	}
	
	pid := *_waitpid;
	if (pid != 0){
		go func() {
			err := WaitForProcessExit(pid)
			if err != nil {
				fmt.Println("Error waiting for process:", err)
			} else {
				fmt.Println("Process exited.")
			}
			os.Exit(1) // Exit the whole program when done
		}()
	}
	
	
	runtimeDebug.SetMemoryLimit(2 * 1024 * 1024 * 1024) // 2GB
	go func() {
		var memStats runtime.MemStats
		for {
			time.Sleep(2 * time.Second)
			runtime.ReadMemStats(&memStats)
			if memStats.HeapAlloc > 1.5*1024*1024*1024 {
				// too much memory for sing-box, crash
				panic("Memory has reached 1.5 GB, this is not normal")
			}
		}
	}()
	
	if runtime.GOOS == "windows" {
		if *_admin{
			code, err := runAdmin(_port, _debug)
			if (err != nil){
				fmt.Fprintf(os.Stderr, "Failed to run as admin: %v\n", err)
			}
			os.Exit(code)
		}
	}
	
	fmt.Println("sing-box:", C.Version)
	fmt.Println()

	testCtx, cancelTests = context.WithCancel(context.Background())

	RunCore(_port, _debug)
	return
}
