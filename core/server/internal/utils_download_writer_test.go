package internal

import (
	"errors"
	"strings"
	"testing"
)

type downloadWriteCloser struct {
	writeErr error
	closeErr error
	closed   bool
	contents strings.Builder
}

func (w *downloadWriteCloser) Write(p []byte) (int, error) {
	if w.writeErr != nil {
		return 0, w.writeErr
	}
	return w.contents.Write(p)
}

func (w *downloadWriteCloser) Close() error {
	w.closed = true
	return w.closeErr
}

func TestCopyAndCloseDownloadReportsFileErrors(t *testing.T) {
	writeErr := errors.New("disk full")
	closeErr := errors.New("close failed")
	for _, tc := range []struct {
		name                     string
		writeErr, closeErr, want error
	}{
		{"success", nil, nil, nil},
		{"write error", writeErr, nil, writeErr},
		{"close error", nil, closeErr, closeErr},
		{"both errors", writeErr, closeErr, writeErr},
	} {
		t.Run(tc.name, func(t *testing.T) {
			out := &downloadWriteCloser{writeErr: tc.writeErr, closeErr: tc.closeErr}
			err := copyAndCloseDownload(out, strings.NewReader("complete"))
			if !errors.Is(err, tc.want) {
				t.Fatalf("error = %v; want %v", err, tc.want)
			}
			if !out.closed {
				t.Error("output not closed")
			}
			if tc.writeErr == nil && out.contents.String() != "complete" {
				t.Error("payload not written completely")
			}
		})
	}
}
