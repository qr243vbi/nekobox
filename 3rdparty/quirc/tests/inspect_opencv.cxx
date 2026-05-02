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
#include <string.h>

#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

using namespace cv;

#include "quirc_internal.h"
#include "dbgutil.h"

/*
 * Macros to convert a color from the SDL/SDL_gfxprimitive style format.
 *
 * Note: we don't use alpha values and thus ignore them.
 */

/*
 * The format used for a frame with bpp=4, little-endian.
 */

#define RAW_R(c)    ((c) & 0xff)
#define RAW_G(c)    ((c >> 8) & 0xff)
#define RAW_B(c)    ((c >> 16) & 0xff)

/*
 * SDL_gfx color is 0xRRGGBBAA
 * https://www.ferzkopp.net/Software/SDL_gfx-2.0/Docs/html/index.html
 */

#define	R(c)	((c >> 24) & 0xff)
#define	G(c)	((c >> 16) & 0xff)
#define	B(c)	((c >> 8) & 0xff)
#define	A(c)	(c & 0xff)

/*
 * OpenCV color is BGR.
 */

#define	COLOR(c)	Scalar(B(c), G(c), R(c))

static double scale = 1.0;

static void stringColor(Mat &screen, int x, int y, const char *text,
			uint32_t color)
{
	static const int font = FONT_HERSHEY_PLAIN;
	static const int thickness = scale;
	static const double font_scale = scale;

	putText(screen, text, Point(x, y), font, font_scale, COLOR(color),
		thickness);
}

static void pixelColor(Mat &screen, int x, int y, uint32_t color)
{
	if (x < 0 || y < 0 || x >= screen.cols || y >= screen.rows) {
		return;
	}

	Vec3b &screen_pixel = screen.at<Vec3b>(y, x);
	screen_pixel[0] = B(color);
	screen_pixel[1] = G(color);
	screen_pixel[2] = R(color);
}

static void pixelColorRaw(Mat &screen, int x, int y, uint32_t rawColor)
{
	if (x < 0 || y < 0 || x >= screen.cols || y >= screen.rows) {
		return;
	}

	Vec3b &screen_pixel = screen.at<Vec3b>(y, x);
	screen_pixel[0] = RAW_B(rawColor);
	screen_pixel[1] = RAW_G(rawColor);
	screen_pixel[2] = RAW_R(rawColor);
}

static void lineColor(Mat &screen, int x1, int y1, int x2, int y2,
		      uint32_t color)
{
	line(screen, Point(x1, y1), Point(x2, y2), COLOR(color), scale);
}

static void dump_info(struct quirc *q)
{
	int count = quirc_count(q);
	int i;

	printf("%d QR-codes found:\n\n", count);
	for (i = 0; i < count; i++) {
		struct quirc_code code;
		struct quirc_data data;
		quirc_decode_error_t err;

		quirc_extract(q, i, &code);
		err = quirc_decode(&code, &data);
		if (err == QUIRC_ERROR_DATA_ECC) {
			quirc_flip(&code);
			err = quirc_decode(&code, &data);
		}

		dump_cells(&code);
		printf("\n");

		if (err) {
			printf("  Decoding FAILED: %s\n", quirc_strerror(err));
		} else {
			printf("  Decoding successful:\n");
			dump_data(&data);
		}

		printf("\n");
	}
}

static void draw_frame(Mat &screen, struct quirc *q)
{
	uint8_t *raw = q->image;
	int x, y;

	for (y = 0; y < q->h; y++) {
		for (x = 0; x < q->w; x++) {
			uint8_t v = *(raw++);
			uint32_t color = (v << 16) | (v << 8) | v;
			struct quirc_region *reg = &q->regions[v];

			switch (v) {
			case QUIRC_PIXEL_WHITE:
				color = 0x00ffffff;
				break;

			case QUIRC_PIXEL_BLACK:
				color = 0x00000000;
				break;

			default:
				if (reg->capstone >= 0)
					color = 0x00008000;
				else
					color = 0x00808080;
				break;
			}

			pixelColorRaw(screen, x, y, color);
		}
	}
}

static void draw_blob(Mat &screen, int x, int y)
{
	int i, j;

	for (i = -2; i <= 2; i++)
		for (j = -2; j <= 2; j++)
			pixelColor(screen, x + i, y + j, 0x0000ffff);
}

static void draw_mark(Mat &screen, int x, int y)
{
	pixelColor(screen, x, y, 0xff0000ff);
	pixelColor(screen, x + 1, y, 0xff0000ff);
	pixelColor(screen, x - 1, y, 0xff0000ff);
	pixelColor(screen, x, y + 1, 0xff0000ff);
	pixelColor(screen, x, y - 1, 0xff0000ff);
}

static void draw_capstone(Mat &screen, struct quirc *q, int index)
{
	struct quirc_capstone *cap = &q->capstones[index];
	int j;
	char buf[8];

	for (j = 0; j < 4; j++) {
		struct quirc_point *p0 = &cap->corners[j];
		struct quirc_point *p1 = &cap->corners[(j + 1) % 4];

		lineColor(screen, p0->x, p0->y, p1->x, p1->y,
			  0x800080ff);
	}

	draw_blob(screen, cap->corners[0].x, cap->corners[0].y);

	if (cap->qr_grid < 0) {
		snprintf(buf, sizeof(buf), "?%d", index);
		stringColor(screen, cap->center.x, cap->center.y, buf,
			    0x000000ff);
	}
}

static void perspective_map(const double *c,
			    double u, double v, struct quirc_point *ret)
{
	double den = c[6]*u + c[7]*v + 1.0;
	double x = (c[0]*u + c[1]*v + c[2]) / den;
	double y = (c[3]*u + c[4]*v + c[5]) / den;

	ret->x = rint(x);
	ret->y = rint(y);
}

static void draw_grid(Mat &screen, struct quirc *q, int index)
{
	struct quirc_grid *qr = &q->grids[index];
	int x, y;
	int i;

	for (i = 0; i < 3; i++) {
		struct quirc_capstone *cap = &q->capstones[qr->caps[i]];
		char buf[16];

		snprintf(buf, sizeof(buf), "%d.%c", index, "ABC"[i]);
		stringColor(screen, cap->center.x, cap->center.y, buf,
			    0x000000ff);
	}

	lineColor(screen, qr->tpep[0].x, qr->tpep[0].y,
		  qr->tpep[1].x, qr->tpep[1].y, 0xff00ffff);
	lineColor(screen, qr->tpep[1].x, qr->tpep[1].y,
		  qr->tpep[2].x, qr->tpep[2].y, 0xff00ffff);

	if (qr->align_region >= 0)
		draw_blob(screen, qr->align.x, qr->align.y);

	for (y = 0; y < qr->grid_size; y++) {
		for (x = 0; x < qr->grid_size; x++) {
			double u = x + 0.5;
			double v = y + 0.5;
			struct quirc_point p;

			perspective_map(qr->c, u, v, &p);
			draw_mark(screen, p.x, p.y);
		}
	}
}

static int opencv_examine(struct quirc *q)
{
	static const char *win = "inspect-opencv";
	Mat frame(q->h, q->w, CV_8UC3);
	int i;

	draw_frame(frame, q);
	for (i = 0; i < q->num_capstones; i++)
	    draw_capstone(frame, q, i);
	for (i = 0; i < q->num_grids; i++)
	    draw_grid(frame, q, i);

	imshow(win, frame);
	waitKey(0);

	return 0;
}

int main(int argc, char **argv)
{
	struct quirc *q;

	printf("quirc inspection program\n");
	printf("Copyright (C) 2010-2012 Daniel Beer <dlbeer@gmail.com>\n");
	printf("Library version: %s\n", quirc_version());
	printf("\n");

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <testfile.jpg|testfile.png>\n", argv[0]);
		return -1;
	}

	q = quirc_new();
	if (!q) {
		perror("can't create quirc object");
		return -1;
	}

	int status = -1;
	if (check_if_png(argv[1])) {
		status = load_png(q, argv[1]);
	} else {
		status = load_jpeg(q, argv[1]);
	}
	if (status < 0) {
		quirc_destroy(q);
		return -1;
	}

	quirc_end(q);
	dump_info(q);

	if (opencv_examine(q) < 0) {
		quirc_destroy(q);
		return -1;
	}

	quirc_destroy(q);
	return 0;
}
