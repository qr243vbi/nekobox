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

#include <stdio.h>
#include <ctype.h>

#include "demoutil.h"

void print_data(const struct quirc_data *data, struct dthash *dt,
		int want_verbose)
{
	if (dthash_seen(dt, data))
		return;

	printf("==> %s\n", data->payload);

	if (want_verbose)
		printf("    Version: %d, ECC: %c, Mask: %d, Type: %d\n\n",
		       data->version, "MLHQ"[data->ecc_level],
		       data->mask, data->data_type);
}

int parse_size(const char *text, int *video_width, int *video_height)
{
	int state = 0;
	int w = 0, h = 0;
	int i;

	for (i = 0; text[i]; i++) {
		if (text[i] == 'x' || text[i] == 'X') {
			if (state == 0) {
				state = 1;
			} else {
				fprintf(stderr, "parse_size: expected WxH\n");
				return -1;
			}
		} else if (isdigit(text[i])) {
			if (state == 0)
				w = w * 10 + text[i] - '0';
			else
				h = h * 10 + text[i] - '0';
		} else {
			fprintf(stderr, "Invalid character in size: %c\n",
				text[i]);
			return -1;
		}
	}

	if (w <= 0 || w >= 10000 || h <= 0 || h >= 10000) {
		fprintf(stderr, "Invalid size: %dx%d\n", w, h);
		return -1;
	}

	*video_width = w;
	*video_height = h;

	return 0;
}
