//go:build freebsd
package main

import (
	"context"
	"nekobox_core/gen"
)

func (s *server) SetSystemDNS(ctx context.Context, in *gen.SetSystemDNSRequest) (*gen.EmptyResp, error) {
	return &gen.EmptyResp{}, nil
}

func runAdmin() (int, error) { return 0, nil }

func checkTaskScheduler(save bool) error { return nil }

func doRun(_ string) error { return nil }

func (s *server) IsPrivileged(ctx context.Context, in *gen.EmptyReq) (*gen.IsPrivilegedResponse, error) {
	return &gen.IsPrivilegedResponse{HasPrivilege: false}, nil
}

func WaitForProcessExit(pid int) error { return nil }
