package auth

import "context"

type userKey struct{}

func ContextWithUser[T any](ctx context.Context, user T) context.Context {
	return context.WithValue(ctx, (*userKey)(nil), user)
}

func UserFromContext[T any](ctx context.Context) (T, bool) {
	user, loaded := ctx.Value((*userKey)(nil)).(T)
	return user, loaded
}
