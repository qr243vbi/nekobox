package json

import (
	"bytes"
	"context"
	"errors"
	"strings"

	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
)

func UnmarshalExtended[T any](content []byte) (T, error) {
	return UnmarshalExtendedContext[T](context.Background(), content)
}

func UnmarshalExtendedContext[T any](ctx context.Context, content []byte) (T, error) {
	decoder := NewDecoderContext(ctx, NewCommentFilter(bytes.NewReader(content)))
	var value T
	err := decoder.Decode(&value)
	if err == nil {
		return value, err
	}
	var syntaxError *SyntaxError
	if errors.As(err, &syntaxError) {
		prefix := string(content[:syntaxError.Offset])
		row := strings.Count(prefix, "\n") + 1
		column := len(prefix) - strings.LastIndex(prefix, "\n") - 1
		return common.DefaultValue[T](), E.Extend(syntaxError, "row ", row, ", column ", column)
	}
	return common.DefaultValue[T](), err
}
