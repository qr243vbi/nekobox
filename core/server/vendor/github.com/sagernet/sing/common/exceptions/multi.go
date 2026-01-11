package exceptions

import (
	"errors"
	"strings"

	"github.com/sagernet/sing/common"
	F "github.com/sagernet/sing/common/format"
)

type multiError struct {
	errors []error
}

func (e *multiError) Error() string {
	return "(" + strings.Join(F.MapToString(e.errors), " | ") + ")"
}

func (e *multiError) Unwrap() []error {
	return e.errors
}

func Errors(errors ...error) error {
	errors = common.FilterNotNil(errors)
	errors = ExpandAll(errors)
	errors = common.FilterNotNil(errors)
	errors = common.UniqBy(errors, error.Error)
	switch len(errors) {
	case 0:
		return nil
	case 1:
		return errors[0]
	}
	return &multiError{
		errors: errors,
	}
}

func Expand(err error) []error {
	if err == nil {
		return nil
	} else if multiErr, isMultiErr := err.(MultiError); isMultiErr {
		return ExpandAll(common.FilterNotNil(multiErr.Unwrap()))
	} else {
		return []error{err}
	}
}

func ExpandAll(errs []error) []error {
	return common.FlatMap(errs, Expand)
}

func Append(err error, other error, block func(error) error) error {
	if other == nil {
		return err
	}
	return Errors(err, block(other))
}

func IsMulti(err error, targetList ...error) bool {
	for _, target := range targetList {
		if errors.Is(err, target) {
			return true
		}
	}
	return false
}
