//go:build !windows

package windnsapi

import "os"

func FlushResolverCache() error {
	return os.ErrInvalid
}
