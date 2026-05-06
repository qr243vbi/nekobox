//go:build !windows

/* SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2017-2023 WireGuard LLC. All Rights Reserved.
 */

package conn

import "github.com/sagernet/sing/common/control"

func NewDefaultBind(externalControl control.Func) Bind {
	return NewStdNetBind(externalControl)
}
