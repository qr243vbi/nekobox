package common

import "io"

type closeWrapper struct {
	closer func() error
}

func (w *closeWrapper) Close() error {
	return w.closer()
}

func Closer(closer func() error) io.Closer {
	if closer == nil {
		return nil
	}
	return &closeWrapper{closer}
}
