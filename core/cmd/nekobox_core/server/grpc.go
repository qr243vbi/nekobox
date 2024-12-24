package grpc_server

import (
	"context"
	"flag"
	"log"
	"nekobox_core/server/gen"
	"net"
	"os"
	"runtime"
	"strconv"
	"syscall"
	"time"

	"github.com/matsuridayo/libneko/neko_common"
	"google.golang.org/grpc"
)

type BaseServer struct {
	gen.LibcoreServiceServer
}

func (s *BaseServer) Exit(ctx context.Context, in *gen.EmptyReq) (out *gen.EmptyResp, _ error) {
	out = &gen.EmptyResp{}

	// Connection closed
	os.Exit(0)
	return
}

func RunCore(setupCore func(), server gen.LibcoreServiceServer) {
	_port := flag.Int("port", 19810, "")
	_debug := flag.Bool("debug", false, "")
	flag.CommandLine.Parse(os.Args[2:])

	neko_common.Debug = *_debug

	go func() {
		parent, err := os.FindProcess(os.Getppid())
		if err != nil {
			log.Fatalln("find parent:", err)
		}
		if runtime.GOOS == "windows" {
			state, err := parent.Wait()
			log.Fatalln("parent exited:", state, err)
		} else {
			for {
				time.Sleep(time.Second * 10)
				err = parent.Signal(syscall.Signal(0))
				if err != nil {
					log.Fatalln("parent exited:", err)
				}
			}
		}
	}()

	// Libcore
	setupCore()

	// GRPC
	lis, err := net.Listen("tcp", "127.0.0.1:"+strconv.Itoa(*_port))
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}

	s := grpc.NewServer(
		grpc.MaxRecvMsgSize(1024*1024*1024), // 1 gigaByte
		grpc.MaxSendMsgSize(1024*1024*1024), // 1 gigaByte
	)
	gen.RegisterLibcoreServiceServer(s, server)

	name := "Core"

	log.Printf("%s grpc server listening at %v\n", name, lis.Addr())
	if err := s.Serve(lis); err != nil {
		log.Fatalf("failed to serve: %v", err)
	}
}
