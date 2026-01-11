package pause

import (
	"context"

	"github.com/sagernet/sing/service"
)

// Deprecated: use service.ContextWith instead.
func ManagerFromContext(ctx context.Context) Manager {
	return service.FromContext[Manager](ctx)
}

// Deprecated: use service.ContextWith instead.
func ContextWithManager(ctx context.Context, manager Manager) context.Context {
	return service.ContextWith[Manager](ctx, manager)
}

// Deprecated: use WithDefaultManager instead.
func ContextWithDefaultManager(ctx context.Context) context.Context {
	return WithDefaultManager(ctx)
}
