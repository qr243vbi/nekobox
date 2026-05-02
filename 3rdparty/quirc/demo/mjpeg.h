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

#ifndef MJPEG_H_
#define MJPEG_H_

#include <stdint.h>
#include <stdio.h>
#include <jpeglib.h>
#include <setjmp.h>

struct mjpeg_decoder {
	/* The error manager must be the first item in this struct */
	struct jpeg_error_mgr			err;
	struct jpeg_decompress_struct		dinfo;
	struct jpeg_source_mgr			src;
	jmp_buf					env;
};

/* Construct an MJPEG decoder. */
void mjpeg_init(struct mjpeg_decoder *mj);

/* Free any memory allocated while decoding MJPEG frames. */
void mjpeg_free(struct mjpeg_decoder *mj);

/* Decode a single MJPEG image to the buffer given, in RGB format.
 * Returns 0 on success, -1 if an error occurs (bad data, or image too
 * big for buffer).
 */
int mjpeg_decode_rgb32(struct mjpeg_decoder *mj,
		       const uint8_t *data, int datalen,
		       uint8_t *out, int pitch, int max_w, int max_h);

/* Decode a single MJPEG image to the buffer given in 8-bit grayscale.
 * Returns 0 on success, -1 if an error occurs.
 */
int mjpeg_decode_gray(struct mjpeg_decoder *mj,
		      const uint8_t *data, int datalen,
		      uint8_t *out, int pitch, int max_w, int max_h);

#endif
