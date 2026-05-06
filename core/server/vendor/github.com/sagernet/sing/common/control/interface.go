package control

import (
	"syscall"

	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
)

type Func = func(network, address string, conn syscall.RawConn) error

func Append(oldFunc Func, newFunc Func) Func {
	if oldFunc == nil {
		return newFunc
	} else if newFunc == nil {
		return oldFunc
	}
	return func(network, address string, conn syscall.RawConn) error {
		if err := oldFunc(network, address, conn); err != nil {
			return err
		}
		return newFunc(network, address, conn)
	}
}

func Conn(conn syscall.Conn, block func(fd uintptr) error) error {
	rawConn, err := conn.SyscallConn()
	if err != nil {
		return err
	}
	return Raw(rawConn, block)
}

func Conn0[T any](conn syscall.Conn, block func(fd uintptr) (T, error)) (T, error) {
	rawConn, err := conn.SyscallConn()
	if err != nil {
		return common.DefaultValue[T](), err
	}
	return Raw0[T](rawConn, block)
}

func Raw(rawConn syscall.RawConn, block func(fd uintptr) error) error {
	var innerErr error
	err := rawConn.Control(func(fd uintptr) {
		innerErr = block(fd)
	})
	return E.Errors(innerErr, err)
}

func Raw0[T any](rawConn syscall.RawConn, block func(fd uintptr) (T, error)) (T, error) {
	var (
		value    T
		innerErr error
	)
	err := rawConn.Control(func(fd uintptr) {
		value, innerErr = block(fd)
	})
	return value, E.Errors(innerErr, err)
}
