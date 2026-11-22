package main

import (
	"context"

	"nekobox_core/gen"
)

// serviceMode is true only while running as "nekobox_core service run".
// In every other mode Start/Stop behave exactly as before.
var serviceMode bool

func (s *server) Start(ctx context.Context, in *gen.LoadConfigReq) (*gen.ErrorResp, error) {
	if serviceMode {
		return s.serviceStart(ctx, in)
	}
	return s.startInternal(ctx, in)
}

func (s *server) Stop(ctx context.Context, in *gen.EmptyReq) (*gen.ErrorResp, error) {
	if serviceMode {
		return s.serviceStop(ctx, in)
	}
	return s.stopInternal(ctx, in)
}
