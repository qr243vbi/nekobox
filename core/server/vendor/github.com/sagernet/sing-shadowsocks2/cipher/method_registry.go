package cipher

import (
	"context"

	E "github.com/sagernet/sing/common/exceptions"
)

var methodRegistry map[string]MethodCreator

func RegisterMethod(methods []string, creator MethodCreator) {
	if methodRegistry == nil {
		methodRegistry = make(map[string]MethodCreator)
	}
	for _, method := range methods {
		methodRegistry[method] = creator
	}
}

func CreateMethod(ctx context.Context, methodName string, options MethodOptions) (Method, error) {
	if methodRegistry == nil {
		methodRegistry = make(map[string]MethodCreator)
	}
	creator, ok := methodRegistry[methodName]
	if !ok {
		return nil, E.New("unknown method: ", methodName)
	}
	return creator(ctx, methodName, options)
}
