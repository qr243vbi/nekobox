package service

import (
	"context"

	"github.com/sagernet/sing/common"
)

func ContextWithRegistry(ctx context.Context, registry Registry) context.Context {
	return context.WithValue(ctx, common.DefaultValue[*Registry](), registry)
}

func ContextWithDefaultRegistry(ctx context.Context) context.Context {
	if RegistryFromContext(ctx) != nil {
		return ctx
	}
	return context.WithValue(ctx, common.DefaultValue[*Registry](), NewRegistry())
}

func RegistryFromContext(ctx context.Context) Registry {
	registry := ctx.Value(common.DefaultValue[*Registry]())
	if registry == nil {
		return nil
	}
	return registry.(Registry)
}

func FromContext[T any](ctx context.Context) T {
	registry := RegistryFromContext(ctx)
	if registry == nil {
		return common.DefaultValue[T]()
	}
	service := registry.Get(common.DefaultValue[*T]())
	if service == nil {
		return common.DefaultValue[T]()
	}
	return service.(T)
}

func PtrFromContext[T any](ctx context.Context) *T {
	registry := RegistryFromContext(ctx)
	if registry == nil {
		return nil
	}
	servicePtr := registry.Get(common.DefaultValue[*T]())
	if servicePtr == nil {
		return nil
	}
	return servicePtr.(*T)
}

func ContextWith[T any](ctx context.Context, service T) context.Context {
	registry := RegistryFromContext(ctx)
	if registry == nil {
		registry = NewRegistry()
		ctx = ContextWithRegistry(ctx, registry)
	}
	registry.Register(common.DefaultValue[*T](), service)
	return ctx
}

func ContextWithPtr[T any](ctx context.Context, servicePtr *T) context.Context {
	registry := RegistryFromContext(ctx)
	if registry == nil {
		registry = NewRegistry()
		ctx = ContextWithRegistry(ctx, registry)
	}
	registry.Register(common.DefaultValue[*T](), servicePtr)
	return ctx
}

func MustRegister[T any](ctx context.Context, service T) {
	registry := RegistryFromContext(ctx)
	if registry == nil {
		panic("missing service registry in context")
	}
	registry.Register(common.DefaultValue[*T](), service)
}

func MustRegisterPtr[T any](ctx context.Context, servicePtr *T) {
	registry := RegistryFromContext(ctx)
	if registry == nil {
		panic("missing service registry in context")
	}
	registry.Register(common.DefaultValue[*T](), servicePtr)
}
