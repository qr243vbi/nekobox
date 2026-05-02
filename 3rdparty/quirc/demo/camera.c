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

#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#ifdef __OpenBSD__
#include <sys/videoio.h>
#else
#include <linux/videodev2.h>
#endif
#include "camera.h"

/************************************************************************
 * Fraction arithmetic
 */

static int gcd(int a, int b)
{
	if (a < 0)
		a = -a;
	if (b < 0)
		b = -b;

	for (;;) {
		if (a < b) {
			const int t = a;

			a = b;
			b = t;
		}

		if (!b)
			break;

		a %= b;
	}

	return a;
}

static void frac_reduce(const struct v4l2_fract *f, struct v4l2_fract *g)
{
	const int x = gcd(f->numerator, f->denominator);
	int n = f->numerator;
	int d = f->denominator;

	if (d < 0) {
		n = -n;
		d = -d;
	}

	g->numerator = n / x;
	g->denominator = d / x;
}

static void frac_add(const struct v4l2_fract *a, const struct v4l2_fract *b,
		     struct v4l2_fract *r)
{
	r->numerator = a->numerator * b->denominator +
		       b->numerator * b->denominator;
	r->denominator = a->denominator * b->denominator;

	frac_reduce(r, r);
}

static void frac_sub(const struct v4l2_fract *a, const struct v4l2_fract *b,
		     struct v4l2_fract *r)
{
	r->numerator = a->numerator * b->denominator -
		       b->numerator * b->denominator;
	r->denominator = a->denominator * b->denominator;

	frac_reduce(r, r);
}

static int frac_cmp(const struct v4l2_fract *a, const struct v4l2_fract *b)
{
	return a->numerator * b->denominator - b->numerator * b->denominator;
}

static void frac_mul(const struct v4l2_fract *a, const struct v4l2_fract *b,
		     struct v4l2_fract *r)
{
	r->numerator = a->numerator * b->numerator;
	r->denominator = a->denominator * b->denominator;

	frac_reduce(r, r);
}

static void frac_div(const struct v4l2_fract *a, const struct v4l2_fract *b,
		     struct v4l2_fract *r)
{
	r->numerator = a->numerator * b->denominator;
	r->denominator = a->denominator * b->numerator;

	frac_reduce(r, r);
}

static int frac_cmp_ref(const struct v4l2_fract *ref,
			const struct v4l2_fract *a,
			const struct v4l2_fract *b)
{
	struct v4l2_fract da;
	struct v4l2_fract db;

	frac_sub(a, ref, &da);
	frac_sub(b, ref, &db);

	if (da.numerator < 0)
		da.numerator = -da.numerator;
	if (db.numerator < 0)
		db.numerator = -db.numerator;

	return frac_cmp(&da, &db);
}

/************************************************************************
 * Parameter searching and choosing
 */

static camera_format_t map_fmt(uint32_t pf)
{
	if (pf == V4L2_PIX_FMT_YUYV)
		return CAMERA_FORMAT_YUYV;

	if (pf == V4L2_PIX_FMT_MJPEG)
		return CAMERA_FORMAT_MJPEG;

	return CAMERA_FORMAT_UNKNOWN;
}

static int find_best_format(int fd, uint32_t *fmt_ret)
{
	struct v4l2_fmtdesc best;
	int i = 1;

	memset(&best, 0, sizeof(best));
	best.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	best.index = 0;

	if (ioctl(fd, VIDIOC_ENUM_FMT, &best) < 0)
		return -1;

	for (;;) {
		struct v4l2_fmtdesc f;

		memset(&f, 0, sizeof(f));
		f.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		f.index = ++i;

		if (ioctl(fd, VIDIOC_ENUM_FMT, &f) < 0)
			break;

		if (map_fmt(f.pixelformat) > map_fmt(best.pixelformat))
			memcpy(&best, &f, sizeof(best));
	}

	if (fmt_ret)
		*fmt_ret = best.pixelformat;

	return 0;
}

static int step_to_discrete(int min, int max, int step, int target)
{
	int offset;

	if (target < min)
		return min;

	if (target > max)
		return max;

	offset = (target - min) % step;
	if ((offset * 2 > step) && (target + step <= max))
		target += step;

	return target - offset;
}

static int score_discrete(const struct v4l2_frmsizeenum *f, int w, int h)
{
	const int dw = f->discrete.width - w;
	const int dh = f->discrete.height - h;

	return dw * dw + dh * dh;
}

static int find_best_size(int fd, uint32_t pixel_format,
			  int target_w, int target_h,
			  int *ret_w, int *ret_h)
{
	struct v4l2_frmsizeenum best;
	int i = 1;

	memset(&best, 0, sizeof(best));
	best.index = 0;
	best.pixel_format = pixel_format;

	if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &best) < 0)
		return -1;

	if (best.type != V4L2_FRMSIZE_TYPE_DISCRETE) {
		*ret_w = step_to_discrete(best.stepwise.min_width,
					  best.stepwise.max_width,
					  best.stepwise.step_width,
					  target_w);
		*ret_h = step_to_discrete(best.stepwise.min_height,
					  best.stepwise.max_height,
					  best.stepwise.step_height,
					  target_h);
		return 0;
	}

	for (;;) {
		struct v4l2_frmsizeenum f;

		memset(&f, 0, sizeof(f));
		f.index = ++i;
		f.pixel_format = pixel_format;

		if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &f) < 0)
			break;

		if (score_discrete(&f, target_w, target_h) <
		    score_discrete(&best, target_w, target_h))
			memcpy(&best, &f, sizeof(best));
	}

	*ret_w = best.discrete.width;
	*ret_h = best.discrete.height;

	return 0;
}

static int find_best_rate(int fd, uint32_t pixel_format,
			  int w, int h, int target_n, int target_d,
			  int *ret_n, int *ret_d)
{
	const struct v4l2_fract target = {
		.numerator = target_n,
		.denominator = target_d
	};
	struct v4l2_frmivalenum best;
	int i = 1;

	memset(&best, 0, sizeof(best));
	best.index = 0;
	best.pixel_format = pixel_format;
	best.width = w;
	best.height = h;

	if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &best) < 0)
		return -1;

	if (best.type != V4L2_FRMIVAL_TYPE_DISCRETE) {
		struct v4l2_fract t;

		if (frac_cmp(&target, &best.stepwise.min) < 0) {
			*ret_n = best.stepwise.min.numerator;
			*ret_d = best.stepwise.min.denominator;
		}

		if (frac_cmp(&target, &best.stepwise.max) > 0) {
			*ret_n = best.stepwise.max.numerator;
			*ret_d = best.stepwise.max.denominator;
		}

		frac_sub(&target, &best.stepwise.min, &t);
		frac_div(&t, &best.stepwise.step, &t);
		if (t.numerator * 2 >= t.denominator)
			t.numerator += t.denominator;
		t.numerator /= t.denominator;
		t.denominator = 1;
		frac_mul(&t, &best.stepwise.step, &t);
		frac_add(&t, &best.stepwise.max, &t);

		if (frac_cmp(&t, &best.stepwise.max) > 0) {
			*ret_n = best.stepwise.max.numerator;
			*ret_d = best.stepwise.max.denominator;
		} else {
			*ret_n = t.numerator;
			*ret_d = t.denominator;
		}

		return 0;
	}

	for (;;) {
		struct v4l2_frmivalenum f;

		memset(&f, 0, sizeof(f));
		f.index = ++i;
		f.pixel_format = pixel_format;
		f.width = w;
		f.height = h;

		if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &f) < 0)
			break;

		if (frac_cmp_ref(&target, &f.discrete, &best.discrete) < 0)
			memcpy(&best, &f, sizeof(best));
	}

	*ret_n = best.discrete.numerator;
	*ret_d = best.discrete.denominator;

	return 0;
}

/************************************************************************
 * Public interface
 */

void camera_init(struct camera *c)
{
	c->fd = -1;
	c->buf_count = 0;
	c->s_on = 0;
}

void camera_destroy(struct camera *c)
{
	camera_close(c);
}

int camera_open(struct camera *c, const char *path,
		int target_w, int target_h,
		int tr_n, int tr_d)
{
	struct v4l2_format fmt;
	struct v4l2_streamparm parm;
	uint32_t pf;
	int w, h;
	int n, d;

	if (c->fd >= 0)
		camera_close(c);

	/* Open device and get basic properties */
	c->fd = open(path, O_RDWR);
	if (c->fd < 0)
		return -1;

	/* Find a pixel format from the list */
	if (find_best_format(c->fd, &pf) < 0)
		goto fail;

	/* Find a frame size */
	if (find_best_size(c->fd, pf, target_w, target_h, &w, &h) < 0)
		goto fail;

	/* Find a frame rate */
	if (find_best_rate(c->fd, pf, w, h, tr_n, tr_d, &n, &d) < 0)
		goto fail;

	/* Set format */
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = w;
	fmt.fmt.pix.height = h;
	fmt.fmt.pix.pixelformat = pf;
	if (ioctl(c->fd, VIDIOC_S_FMT, &fmt) < 0)
		goto fail;

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(c->fd, VIDIOC_G_FMT, &fmt) < 0)
		goto fail;

	/* Set frame interval */
	memset(&parm, 0, sizeof(parm));
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.timeperframe.numerator = n;
	parm.parm.capture.timeperframe.denominator = d;
	if (ioctl(c->fd, VIDIOC_S_PARM, &parm) < 0)
		goto fail;

	memset(&parm, 0, sizeof(parm));
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(c->fd, VIDIOC_G_PARM, &parm) < 0)
		goto fail;

	c->parms.format = map_fmt(fmt.fmt.pix.pixelformat);
	c->parms.width = fmt.fmt.pix.width;
	c->parms.height = fmt.fmt.pix.height;
	c->parms.pitch_bytes = fmt.fmt.pix.bytesperline;
	c->parms.interval_n = parm.parm.capture.timeperframe.numerator;
	c->parms.interval_d = parm.parm.capture.timeperframe.denominator;

	return 0;

fail:
	{
		const int e = errno;

		close(c->fd);
		c->fd = -1;
		errno = e;
	}

	return -1;
}

void camera_close(struct camera *c)
{
	camera_off(c);
	camera_unmap(c);

	if (c->fd < 0)
		return;

	close(c->fd);
	c->fd = -1;
}

int camera_map(struct camera *c, int buf_count)
{
	struct v4l2_requestbuffers reqbuf;
	int count;
	int i;

	if (buf_count > CAMERA_MAX_BUFFERS)
		buf_count = CAMERA_MAX_BUFFERS;

	if (buf_count <= 0) {
		errno = EINVAL;
		return -1;
	}

	if (c->fd < 0) {
		errno = EBADF;
		return -1;
	}

	if (c->buf_count)
		camera_unmap(c);

	memset(&reqbuf, 0, sizeof(reqbuf));
	reqbuf.count = buf_count;
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuf.memory = V4L2_MEMORY_MMAP;

	if (ioctl(c->fd, VIDIOC_REQBUFS, &reqbuf) < 0)
		return -1;

	count = reqbuf.count;
	if (count > CAMERA_MAX_BUFFERS)
		count = CAMERA_MAX_BUFFERS;

	/* Query all buffers */
	for (i = 0; i < count; i++) {
		struct v4l2_buffer buf;
		struct camera_buffer *cb = &c->buf_desc[i];

		memset(&buf, 0, sizeof(buf));
		buf.index = i;
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		if (ioctl(c->fd, VIDIOC_QUERYBUF, &buf) < 0)
			return -1;

		cb->offset = buf.m.offset;
		cb->size = buf.length;
		cb->addr = mmap(NULL, cb->size, PROT_READ,
			MAP_SHARED, c->fd, cb->offset);

		if (cb->addr == MAP_FAILED) {
			const int save = errno;

			i--;
			while (i >= 0) {
				cb = &c->buf_desc[i--];
				munmap(cb->addr, cb->size);
			}

			errno = save;
			return -1;
		}
	}

	c->buf_count = count;
	return 0;
}

void camera_unmap(struct camera *c)
{
	int i;

	for (i = 0; i < c->buf_count; i++) {
		struct camera_buffer *cb = &c->buf_desc[i];

		munmap(cb->addr, cb->size);
	}

	c->buf_count = 0;
}

int camera_on(struct camera *c)
{
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (c->s_on)
		return 0;

	if (c->fd < 0) {
		errno = EBADF;
		return -1;
	}

	if (ioctl(c->fd, VIDIOC_STREAMON, &type) < 0)
		return -1;

	c->s_on = 1;
	c->s_qc = 0;
	c->s_qhead = 0;
	return 0;
}

void camera_off(struct camera *c)
{
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (!c->s_on)
		return;

	ioctl(c->fd, VIDIOC_STREAMOFF, &type);
	c->s_on = 0;
}

int camera_enqueue_all(struct camera *c)
{
	while (c->s_qc < c->buf_count) {
		struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));
		buf.index = (c->s_qc + c->s_qhead) % c->buf_count;
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;

		if (ioctl(c->fd, VIDIOC_QBUF, &buf) < 0)
			return -1;

		c->s_qc++;
	}

	return 0;
}

int camera_dequeue_one(struct camera *c)
{
	struct v4l2_buffer buf;

	if (!c->s_qc) {
		errno = EINVAL;
		return -1;
	}

	memset(&buf, 0, sizeof(buf));
	buf.memory = V4L2_MEMORY_MMAP;
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (ioctl(c->fd, VIDIOC_DQBUF, &buf) < 0)
		return -1;

	c->s_qc--;
	if (++c->s_qhead >= c->buf_count)
		c->s_qhead = 0;

	return 0;
}
