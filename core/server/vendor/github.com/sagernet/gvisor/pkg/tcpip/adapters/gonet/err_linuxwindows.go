//go:build linux || windows

package gonet

import "syscall"

var errNoNet = syscall.ENONET
