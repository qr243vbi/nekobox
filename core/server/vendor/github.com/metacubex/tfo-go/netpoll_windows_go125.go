//go:build windows && go1.25

package tfo

import (
	"errors"
	"net"
	"os"
	"sync"
	"syscall"
	"unsafe"

	"golang.org/x/sys/windows"
)

// operation contains superset of data necessary to perform all async IO.
//
// Copied from src/internal/poll/fd_windows.go
type operation struct {
	// Used by IOCP interface, it must be first field
	// of the struct, as our code rely on it.
	o syscall.Overlapped

	// fields used by runtime.netpoll
	runtimeCtx uintptr
	mode       int32

	// fields used only by net package
	fd     *pFD
	buf    syscall.WSABuf
	msg    windows.WSAMsg
	sa     syscall.Sockaddr
	rsa    *syscall.RawSockaddrAny
	rsan   int32
	handle syscall.Handle
	flags  uint32
	qty    uint32
	bufs   []syscall.WSABuf
}

func (o *operation) setEvent() {
	h, err := windows.CreateEvent(nil, 0, 0, nil)
	if err != nil {
		// This shouldn't happen when all CreateEvent arguments are zero.
		panic(err)
	}
	// Set the low bit so that the external IOCP doesn't receive the completion packet.
	o.o.HEvent = syscall.Handle(h | 1)
}

// waitIO waits for the IO operation o to complete.
func waitIO(o *operation) error {
	if o.fd.isBlocking {
		panic("can't wait on blocking operations")
	}
	fd := o.fd
	if !fd.pollable() {
		// The overlapped handle is not added to the runtime poller,
		// the only way to wait for the IO to complete is block until
		// the overlapped event is signaled.
		_, err := syscall.WaitForSingleObject(o.o.HEvent, syscall.INFINITE)
		return err
	}
	// Wait for our request to complete.
	err := fd.pd.wait(int(o.mode), fd.isFile)
	switch err {
	case nil, ErrNetClosing, ErrFileClosing, ErrDeadlineExceeded:
		// No other error is expected.
	default:
		panic("unexpected runtime.netpoll error: " + err.Error())
	}
	return err
}

// cancelIO cancels the IO operation o and waits for it to complete.
func cancelIO(o *operation) {
	fd := o.fd
	if !fd.pollable() {
		return
	}
	// Cancel our request.
	err := syscall.CancelIoEx(fd.Sysfd, &o.o)
	// Assuming ERROR_NOT_FOUND is returned, if IO is completed.
	if err != nil && err != syscall.ERROR_NOT_FOUND {
		// TODO(brainman): maybe do something else, but panic.
		panic(err)
	}
	fd.pd.waitCanceled(int(o.mode))
}

// execIO executes a single IO operation o.
// It supports both synchronous and asynchronous IO.
// o.qty and o.flags are set to zero before calling submit
// to avoid reusing the values from a previous call.
func execIO(o *operation, submit func(o *operation) error) (int, error) {
	fd := o.fd
	// Notify runtime netpoll about starting IO.
	err := fd.pd.prepare(int(o.mode), fd.isFile)
	if err != nil {
		return 0, err
	}
	// Start IO.
	if !fd.isBlocking && o.o.HEvent == 0 && !fd.pollable() {
		// If the handle is opened for overlapped IO but we can't
		// use the runtime poller, then we need to use an
		// event to wait for the IO to complete.
		o.setEvent()
	}
	o.qty = 0
	o.flags = 0
	err = submit(o)
	var waitErr error
	// Blocking operations shouldn't return ERROR_IO_PENDING.
	// Continue without waiting if that happens.
	if !o.fd.isBlocking && (err == syscall.ERROR_IO_PENDING || (err == nil && !o.fd.skipSyncNotif)) {
		// IO started asynchronously or completed synchronously but
		// a sync notification is required. Wait for it to complete.
		waitErr = waitIO(o)
		if waitErr != nil {
			// IO interrupted by "close" or "timeout".
			cancelIO(o)
			// We issued a cancellation request, but the IO operation may still succeeded
			// before the cancellation request runs.
		}
		if fd.isFile {
			err = windows.GetOverlappedResult(windows.Handle(fd.Sysfd), (*windows.Overlapped)(unsafe.Pointer(&o.o)), &o.qty, false)
		} else {
			err = windows.WSAGetOverlappedResult(windows.Handle(fd.Sysfd), (*windows.Overlapped)(unsafe.Pointer(&o.o)), &o.qty, false, &o.flags)
		}
	}
	switch err {
	case syscall.ERROR_OPERATION_ABORTED:
		// ERROR_OPERATION_ABORTED may have been caused by us. In that case,
		// map it to our own error. Don't do more than that, each submitted
		// function may have its own meaning for each error.
		if waitErr != nil {
			// IO canceled by the poller while waiting for completion.
			err = waitErr
		} else if fd.kind == kindPipe && fd.closing() {
			// Close uses CancelIoEx to interrupt concurrent I/O for pipes.
			// If the fd is a pipe and the Write was interrupted by CancelIoEx,
			// we assume it is interrupted by Close.
			err = errClosing(fd.isFile)
		}
	case windows.ERROR_IO_INCOMPLETE:
		// waitIO couldn't wait for the IO to complete.
		if waitErr != nil {
			// The wait error will be more informative.
			err = waitErr
		}
	}
	return int(o.qty), err
}

// fileKind describes the kind of file.
type fileKind byte

const (
	kindNet fileKind = iota
	kindFile
	kindConsole
	kindPipe
	kindFileNet
)

// This package uses the SetFileCompletionNotificationModes Windows
// API to skip calling GetQueuedCompletionStatus if an IO operation
// completes synchronously. There is a known bug where
// SetFileCompletionNotificationModes crashes on some systems (see
// https://support.microsoft.com/kb/2568167 for details).

var socketCanUseSetFileCompletionNotificationModes bool // determines is SetFileCompletionNotificationModes is present and sockets can safely use it

// checkSetFileCompletionNotificationModes verifies that
// SetFileCompletionNotificationModes Windows API is present
// on the system and is safe to use.
// See https://support.microsoft.com/kb/2568167 for details.
func checkSetFileCompletionNotificationModes() {
	err := syscall.LoadSetFileCompletionNotificationModes()
	if err != nil {
		return
	}
	protos := [2]int32{syscall.IPPROTO_TCP, 0}
	var buf [32]syscall.WSAProtocolInfo
	len := uint32(unsafe.Sizeof(buf))
	n, err := syscall.WSAEnumProtocols(&protos[0], &buf[0], &len)
	if err != nil {
		return
	}
	for i := int32(0); i < n; i++ {
		if buf[i].ServiceFlags1&syscall.XP1_IFS_HANDLES == 0 {
			return
		}
	}
	socketCanUseSetFileCompletionNotificationModes = true
}

func init() {
	checkSetFileCompletionNotificationModes()
}

var serverInit sync.Once

func (fd *pFD) Init(net string, pollable bool) error {
	switch net {
	case "file":
		fd.kind = kindFile
	case "console":
		fd.kind = kindConsole
	case "pipe":
		fd.kind = kindPipe
	case "file+net":
		fd.kind = kindFileNet
	default:
		// We don't actually care about the various network types.
		fd.kind = kindNet
	}
	fd.isFile = fd.kind != kindNet
	fd.isBlocking = !pollable
	fd.rop.mode = 'r'
	fd.wop.mode = 'w'
	fd.rop.fd = fd
	fd.wop.fd = fd

	// It is safe to add overlapped handles that also perform I/O
	// outside of the runtime poller. The runtime poller will ignore
	// I/O completion notifications not initiated by us.
	err := fd.pd.init(fd)
	if err != nil {
		return err
	}
	fd.rop.runtimeCtx = fd.pd.runtimeCtx
	fd.wop.runtimeCtx = fd.pd.runtimeCtx
	if fd.kind != kindNet || socketCanUseSetFileCompletionNotificationModes {
		// Non-socket handles can use SetFileCompletionNotificationModes without problems.
		err := syscall.SetFileCompletionNotificationModes(fd.Sysfd,
			syscall.FILE_SKIP_SET_EVENT_ON_HANDLE|syscall.FILE_SKIP_COMPLETION_PORT_ON_SUCCESS,
		)
		fd.skipSyncNotif = err == nil
	}
	return nil
}

// Error values returned by runtime_pollReset and runtime_pollWait.
// These must match the values in runtime/netpoll.go.
const (
	pollNoError        = 0
	pollErrClosing     = 1
	pollErrTimeout     = 2
	pollErrNotPollable = 3
)

func convertErr(res int, isFile bool) error {
	switch res {
	case pollNoError:
		return nil
	case pollErrClosing:
		return errClosing(isFile)
	case pollErrTimeout:
		return ErrDeadlineExceeded
	case pollErrNotPollable:
		return ErrNotPollable
	}
	println("unreachable: ", res)
	panic("unreachable")
}

// ErrNotPollable is returned when the file or socket is not suitable
// for event notification.
var ErrNotPollable = errors.New("not pollable")

// ErrFileClosing is returned when a file descriptor is used after it
// has been closed.
var ErrFileClosing = errors.New("use of closed file")

// ErrNetClosing is returned when a network descriptor is used after
// it has been closed.
var ErrNetClosing = net.ErrClosed

// ErrDeadlineExceeded is returned for an expired deadline.
var ErrDeadlineExceeded error = os.ErrDeadlineExceeded

// Return the appropriate closing error based on isFile.
func errClosing(isFile bool) error {
	if isFile {
		return os.ErrClosed
	}
	return ErrNetClosing
}

//go:linkname runtime_pollServerInit internal/poll.runtime_pollServerInit
func runtime_pollServerInit()

//go:linkname runtime_pollOpen internal/poll.runtime_pollOpen
func runtime_pollOpen(fd uintptr) (uintptr, int)

//go:linkname runtime_pollWait internal/poll.runtime_pollWait
func runtime_pollWait(ctx uintptr, mode int) int

//go:linkname runtime_pollWaitCanceled internal/poll.runtime_pollWaitCanceled
func runtime_pollWaitCanceled(ctx uintptr, mode int)

//go:linkname runtime_pollReset internal/poll.runtime_pollReset
func runtime_pollReset(ctx uintptr, mode int) int

// Stay in sync with pollDesc in src/internal/poll/fd_poll_runtime.go
type pollDesc struct {
	runtimeCtx uintptr
}

func (pd *pollDesc) init(fd *pFD) error {
	serverInit.Do(runtime_pollServerInit)
	ctx, errno := runtime_pollOpen(uintptr(fd.Sysfd))
	if errno != 0 {
		return syscall.Errno(errno)
	}
	pd.runtimeCtx = ctx
	return nil
}

func (pd *pollDesc) prepare(mode int, isFile bool) error {
	if pd.runtimeCtx == 0 {
		return nil
	}
	res := runtime_pollReset(pd.runtimeCtx, mode)
	return convertErr(res, isFile)
}

func (pd *pollDesc) wait(mode int, isFile bool) error {
	if pd.runtimeCtx == 0 {
		return errors.New("waiting for unsupported file type")
	}
	res := runtime_pollWait(pd.runtimeCtx, mode)
	return convertErr(res, isFile)
}

func (pd *pollDesc) waitCanceled(mode int) {
	if pd.runtimeCtx == 0 {
		return
	}
	runtime_pollWaitCanceled(pd.runtimeCtx, mode)
}

func (pd *pollDesc) pollable() bool {
	return pd.runtimeCtx != 0
}
