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
#include <SDL.h>
#include <SDL_gfxPrimitives.h>
#include "quirc_internal.h"
#include "dbgutil.h"

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

static void draw_frame(SDL_Surface *screen, struct quirc *q)
{
	uint8_t *pix;
	uint8_t *raw = q->image;
	int x, y;

	SDL_LockSurface(screen);
	pix = screen->pixels;
	for (y = 0; y < q->h; y++) {
		uint32_t *row = (uint32_t *)pix;

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

			*(row++) = color;
		}

		pix += screen->pitch;
	}
	SDL_UnlockSurface(screen);
}

static void draw_blob(SDL_Surface *screen, int x, int y)
{
	int i, j;

	for (i = -2; i <= 2; i++)
		for (j = -2; j <= 2; j++)
			pixelColor(screen, x + i, y + j, 0x0000ffff);
}

static void draw_mark(SDL_Surface *screen, int x, int y)
{
	pixelColor(screen, x, y, 0xff0000ff);
	pixelColor(screen, x + 1, y, 0xff0000ff);
	pixelColor(screen, x - 1, y, 0xff0000ff);
	pixelColor(screen, x, y + 1, 0xff0000ff);
	pixelColor(screen, x, y - 1, 0xff0000ff);
}

static void draw_capstone(SDL_Surface *screen, struct quirc *q, int index)
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

static void draw_grid(SDL_Surface *screen, struct quirc *q, int index)
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

static int sdl_examine(struct quirc *q)
{
	SDL_Surface *screen;
	SDL_Event ev;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "couldn't init SDL: %s\n", SDL_GetError());
		return -1;
	}

	screen = SDL_SetVideoMode(q->w, q->h, 32, SDL_SWSURFACE);
	if (!screen) {
		fprintf(stderr, "couldn't init video mode: %s\n",
			SDL_GetError());
		return -1;
	}

	while (SDL_WaitEvent(&ev) >= 0) {
		int i;

		if (ev.type == SDL_QUIT)
			break;

		if (ev.type == SDL_KEYDOWN &&
		    ev.key.keysym.sym == 'q')
			break;

		draw_frame(screen, q);
		for (i = 0; i < q->num_capstones; i++)
			draw_capstone(screen, q, i);
		for (i = 0; i < q->num_grids; i++)
			draw_grid(screen, q, i);
		SDL_Flip(screen);
	}

	SDL_Quit();
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

	if (sdl_examine(q) < 0) {
		quirc_destroy(q);
		return -1;
	}

	quirc_destroy(q);
	return 0;
}
