package boxbox

import (
	"context"
	"fmt"
	"io"
	"reflect"
	"time"
	"unsafe"

	"github.com/sagernet/sing-box/log"
)

func (s *Box) SetLogWritter(w io.Writer) {
	writer_ := reflect.Indirect(reflect.ValueOf(s.logFactory)).FieldByName("writer")
	writer_ = reflect.NewAt(writer_.Type(), unsafe.Pointer(writer_.UnsafeAddr())).Elem()
	writer_.Set(reflect.ValueOf(w))
}

func (s *Box) GetLogPlatformFormatter() *log.Formatter {
	platformFormatter_ := reflect.Indirect(reflect.ValueOf(s.logFactory)).FieldByName("platformFormatter")
	platformFormatter_ = reflect.NewAt(platformFormatter_.Type(), unsafe.Pointer(platformFormatter_.UnsafeAddr()))
	platformFormatter := platformFormatter_.Interface().(*log.Formatter)
	return platformFormatter
}

func (s *Box) CloseWithTimeout(cancal context.CancelFunc, d time.Duration, logFunc func(v ...any)) {
	start := time.Now()
	t := time.NewTimer(d)
	done := make(chan struct{})

	printCloseTime := func() {
		logFunc("[Info] sing-box closed in", fmt.Sprintf("%d ms", time.Since(start).Milliseconds()))
	}

	go func(cancel context.CancelFunc, closer io.Closer) {
		cancel()
		closer.Close()
		close(done)
		if !t.Stop() {
			printCloseTime()
		}
	}(cancal, s)

	select {
	case <-t.C:
		logFunc("[Warning] sing-box close takes longer than expected.")
	case <-done:
		printCloseTime()
	}
}
