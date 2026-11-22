//go:build !windows

package main

import (
	"context"
	"fmt"

	"nekobox_core/gen"
)

// Service mode is Windows-only; these stubs keep other platforms compiling.
func RunServiceMode(args []string) {
	fmt.Println("service mode is only available on Windows")
}

func (s *server) serviceStart(ctx context.Context, in *gen.LoadConfigReq) (*gen.ErrorResp, error) {
	return s.startInternal(ctx, in)
}

func (s *server) serviceStop(ctx context.Context, in *gen.EmptyReq) (*gen.ErrorResp, error) {
	return s.stopInternal(ctx, in)
}
