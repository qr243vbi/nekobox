//go:build !(linux || windows)

package gonet

import "errors"

var errNoNet = errors.New("machine is not on the network")
