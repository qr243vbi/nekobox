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

#ifndef DBGUTIL_H_
#define DBGUTIL_H_

#include "quirc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dump decoded information on stdout. */
void dump_data(const struct quirc_data *data);

/* Dump a grid cell map on stdout. */
void dump_cells(const struct quirc_code *code);

/* Read a JPEG image into the decoder.
 *
 * Note that you must call quirc_end() if the function returns
 * successfully (0).
 */
int load_jpeg(struct quirc *q, const char *filename);

/* Check if a file is a PNG image.
 *
 * returns 1 if the given file is a PNG and 0 otherwise.
 */
int check_if_png(const char *filename);

/* Read a PNG image into the decoder.
 *
 * Note that you must call quirc_end() if the function returns
 * successfully (0).
 */
int load_png(struct quirc *q, const char *filename);

#ifdef __cplusplus
}
#endif

#endif
