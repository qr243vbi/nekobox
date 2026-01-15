package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"nekobox_core/gen"
	"nekobox_core/internal/boxmain"
	_ "nekobox_core/internal/distro/all"
	"net"
	"os"
	"runtime"
	runtimeDebug "runtime/debug"
	"strconv"
	"time"

	main_sing "nekobox_core/gen/main_sing"

	"github.com/apache/thrift/lib/go/thrift"
	C "github.com/sagernet/sing-box/constant"
)

func RunCore(_port *int, _debug *bool) {
	debug = *_debug
	boxmain.DisableColor()
	addr := "127.0.0.1:" + strconv.Itoa(*_port)
	// RPC
	go func() {
		for {
			time.Sleep(100 * time.Millisecond)
			conn, err := net.Dial("tcp", addr)
			if err == nil {
				conn.Close()
				fmt.Printf("Core listening at %v\n", addr)
				return
			}
		}
	}()
	{
		transportFactory := thrift.NewTBufferedTransportFactory(8192)
		config := &thrift.TConfiguration{
			ConnectTimeout: time.Second * 2,  // 2 second connection timeout
			SocketTimeout:  time.Second * 10, // 10 second socket read/write timeout
			MaxMessageSize: 1024 * 1024 * 50, // 50 MB maximum message size
		}

		// 2. Create the TBinaryProtocolFactory using the configuration
		protocolFactory := thrift.NewTBinaryProtocolFactoryConf(config)

		transport, err := thrift.NewTServerSocket(addr)
		if err != nil {
			log.Println("error running thrift server: ", err)
		}
		handler := &server{}
		processor := gen.NewLibcoreServiceProcessor(handler)
		server := thrift.NewTSimpleServer4(processor, transport, transportFactory, protocolFactory)
		server.Serve()
	}
}

func main() {
	if len(os.Args) > 0 {
		if os.Args[1] == "-installer-mode" {
			fmt.Println("nekobox_core installer mode")
			InstallerMode()
			return
		}
                if os.Args[1] == "sing-box" {
			os.Args = os.Args[1:]
			main_sing.MainFunc();
                }
	}

	var _admin *bool
	var _waitpid *int
	_port := flag.Int("port", 19810, "Port")
	_debug := flag.Bool("debug", false, "Debug mode")
	_arg0 := flag.String("argv0", os.Args[0], "Replace first argument")

	if runtime.GOOS == "windows" || runtime.GOOS == "linux" {
		_admin = flag.Bool("admin", false, "Run in admin mode")
	}

	_waitpid = flag.Int("waitpid", 0, "After pid finished, force quit")

	redirectOutput := flag.String("redirect-output", "", "Path to redirect stdout (e.g. named pipe or file)")
	redirectError := flag.String("redirect-error", "", "Path to redirect stderr (e.g. named pipe or file)")

	flag.CommandLine.Parse(os.Args[1:])

	os.Args[0] = *_arg0

	if runtime.GOOS == "linux" {
		if *_admin {
			restartAsAdmin()
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

	pid := *_waitpid
	if pid != 0 {
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
		if *_admin {
			elevated, _ := isElevated()
			if !elevated {
				code, err := runAdmin(_port, _debug)
				if err != nil {
					fmt.Fprintf(os.Stderr, "Failed to run as admin: %v\n", err)
				}
				os.Exit(code)
			}
		}
	}

	fmt.Println("sing-box:", C.Version)
	fmt.Println()

	testCtx, cancelTests = context.WithCancel(context.Background())

	RunCore(_port, _debug)
}
