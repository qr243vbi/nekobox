// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package congestion_bbr1

import (
	"time"

	"github.com/sagernet/quic-go/monotime"
)

// Clock provides the current time.
type Clock interface {
	Now() monotime.Time
}

// DefaultClock is a clock that returns the current monotonic time.
type DefaultClock struct {
	TimeFunc func() time.Time
}

// Now returns the current monotonic time.
func (c DefaultClock) Now() monotime.Time {
	if c.TimeFunc != nil {
		return monotime.Time(c.TimeFunc().UnixNano())
	}
	return monotime.Now()
}
