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

#ifndef DTHASH_H_
#define DTHASH_H_

#include <stdint.h>
#include <time.h>
#include "quirc.h"

/* Detector hash.
 *
 * This structure keeps track of codes that have been seen within the
 * last N seconds, and allows us to print out codes at a reasonable
 * rate as we see them.
 */
#define DTHASH_MAX_CODES	32

struct dthash_code {
	uint32_t		hash;
	time_t			when;
};

struct dthash {
	struct dthash_code	codes[DTHASH_MAX_CODES];
	int			count;
	int			timeout;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise a detector hash with the given timeout. */
void dthash_init(struct dthash *d, int timeout);

/* When a code is discovered, this function should be called to see if
 * it should be printed. The hash will record having seen the code, and
 * return non-zero if it's the first time we've seen it within the
 * configured timeout period.
 */
int dthash_seen(struct dthash *d, const struct quirc_data *data);

#ifdef __cplusplus
}
#endif

#endif
