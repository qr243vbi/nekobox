// Copyright (c) Tailscale Inc & AUTHORS
// SPDX-License-Identifier: BSD-3-Clause

package logtail

import "sync"

// triggerCond is a channel-based condition variable. The Ready method returns
// a channel that is closed when the condition is activated.
//
// A zero triggerCond is ready for use and is inactive, but must not be copied
// after any of its methods have been called.
type triggerCond struct {
	mu     sync.Mutex
	ch     chan struct{}
	closed bool
}

// Set activates the condition. If it was already active, Set has no effect.
func (c *triggerCond) Set() {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.ch == nil {
		c.ch = make(chan struct{})
	}
	if !c.closed {
		close(c.ch)
		c.closed = true
	}
}

// Reset resets the condition. If it was already inactive, Reset has no effect.
func (c *triggerCond) Reset() {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.closed {
		c.ch = nil
		c.closed = false
	}
}

// Ready returns a channel that is closed when c is activated. If c is active
// when Ready is called, the returned channel will already be closed.
func (c *triggerCond) Ready() <-chan struct{} {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.ch == nil {
		c.ch = make(chan struct{})
	}
	return c.ch
}
