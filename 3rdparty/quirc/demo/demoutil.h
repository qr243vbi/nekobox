/* quirc -- QR-code recognition library
 * Copyright (C) 2010-2012 Daniel Beer <dlbeer@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef DEMOUTIL_H_
#define DEMOUTIL_H_

#include "dthash.h"
#include "quirc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Check if we've seen the given code, and if not, print it on stdout.
 * Include version info if requested.
 */
void print_data(const struct quirc_data *data, struct dthash *dt,
		int want_verbose);

/* Parse a string of the form "WxH" and return width and height as
 * integers. Returns 0 on success or -1 if a parser error occurs.
 */
int parse_size(const char *text, int *video_width, int *video_height);

#ifdef __cplusplus
}
#endif

#endif
