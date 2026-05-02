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

#include "convert.h"

#define CHANNEL_CLAMP(dst, tmp, lum, chrom) \
	(tmp) = ((lum) + (chrom)) >> 8; \
	if ((tmp) < 0) \
		(tmp) = 0; \
	if ((tmp) > 255) \
		(tmp) = 255; \
	(dst) = (tmp);

void yuyv_to_rgb32(const uint8_t *src, int src_pitch,
		   int w, int h,
		   uint8_t *dst, int dst_pitch)
{
	int y;

	for (y = 0; y < h; y++) {
		int x;
		const uint8_t *srow = src + y * src_pitch;
		uint8_t *drow = dst + y * dst_pitch;

		for (x = 0; x < w; x += 2) {
			/* ITU-R colorspace assumed */
			int y0 = (int)srow[0] * 256;
			int y1 = (int)srow[2] * 256;
			int cr = (int)srow[3] - 128;
			int cb = (int)srow[1] - 128;
			int r = cr * 359;
			int g = -cb * 88 - 128 * cr;
			int b = 454 * cb;
			int z;

			CHANNEL_CLAMP(drow[0], z, y0, b);
			CHANNEL_CLAMP(drow[1], z, y0, g);
			CHANNEL_CLAMP(drow[2], z, y0, r);
			CHANNEL_CLAMP(drow[4], z, y1, b);
			CHANNEL_CLAMP(drow[5], z, y1, g);
			CHANNEL_CLAMP(drow[6], z, y1, r);

			srow += 4;
			drow += 8;
		}
	}
}

void yuyv_to_luma(const uint8_t *src, int src_pitch,
		  int w, int h,
		  uint8_t *dst, int dst_pitch)
{
	int y;

	for (y = 0; y < h; y++) {
		int x;
		const uint8_t *srow = src + y * src_pitch;
		uint8_t *drow = dst + y * dst_pitch;

		for (x = 0; x < w; x += 2) {
			*(drow++) = srow[0];
			*(drow++) = srow[2];
			srow += 4;
		}
	}
}

void rgb32_to_luma(const uint8_t *src, int src_pitch,
		   int w, int h,
		   uint8_t *dst, int dst_pitch)
{
	int y;

	for (y = 0; y < h; y++) {
		const uint8_t *rgb32 = src + src_pitch * y;
		uint8_t *gray = dst + y * dst_pitch;
		int i;

		for (i = 0; i < w; i++) {
			/* ITU-R colorspace assumed */
			int r = (int)rgb32[2];
			int g = (int)rgb32[1];
			int b = (int)rgb32[0];
			int sum = r * 59 + g * 150 + b * 29;

			*(gray++) = sum >> 8;
			rgb32 += 4;
		}
	}
}
