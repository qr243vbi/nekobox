/* quirc -- QR-code recognition library
 * Copyright (C) 2010-2014 Daniel Beer <dlbeer@gmail.com>
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

#ifndef CAMERA_H_
#define CAMERA_H_

#include <stddef.h>

#define CAMERA_MAX_BUFFERS	32

typedef enum {
	CAMERA_FORMAT_UNKNOWN = 0,
	CAMERA_FORMAT_MJPEG,
	CAMERA_FORMAT_YUYV
} camera_format_t;

struct camera_parms {
	camera_format_t		format;
	int			width;
	int			height;
	int			pitch_bytes;
	int			interval_n;
	int			interval_d;
};

struct camera_buffer {
	void			*addr;
	size_t			size;
	unsigned long		offset;
};

struct camera {
	int			fd;

	struct camera_parms	parms;

	struct camera_buffer	buf_desc[CAMERA_MAX_BUFFERS];
	int			buf_count;

	/* Stream state */
	int			s_on;
	int			s_qc;
	int			s_qhead;
};

/* Initialize/destroy a camera. No resources are allocated. */
void camera_init(struct camera *c);
void camera_destroy(struct camera *c);

/* Open/close the camera device */
int camera_open(struct camera *c, const char *path,
		int target_w, int target_h,
		int tr_n, int tr_d);
void camera_close(struct camera *c);

static inline int camera_get_fd(const struct camera *c)
{
	return c->fd;
}

static inline const struct camera_parms *camera_get_parms
	(const struct camera *c)
{
	return &c->parms;
}

/* Map buffers */
int camera_map(struct camera *c, int buf_count);
void camera_unmap(struct camera *c);

static inline int camera_get_buf_count(const struct camera *c)
{
	return c->buf_count;
}

/* Switch streaming on/off */
int camera_on(struct camera *c);
void camera_off(struct camera *c);

/* Enqueue/dequeue buffers (count = 0 means enqueue all) */
int camera_enqueue_all(struct camera *c);
int camera_dequeue_one(struct camera *c);

/* Fetch the oldest dequeued buffer */
static inline const struct camera_buffer *camera_get_head
	(const struct camera *c)
{
	return &c->buf_desc[(c->s_qhead + c->buf_count - 1) % c->buf_count];
}

#endif
