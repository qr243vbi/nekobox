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

#include <assert.h>
#include <unistd.h>
#include <quirc.h>
#include <time.h>
#include <getopt.h>

#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

using namespace cv;

#include "camera.h"
#include "convert.h"
#include "dthash.h"
#include "demoutil.h"

/* Collected command-line arguments */
static int want_frame_rate = 0;
static int want_verbose = 0;
static int printer_timeout = 2;

static const int font = FONT_HERSHEY_PLAIN;
static const int thickness = 2;
static const double font_scale = 1.5;
static Scalar blue = Scalar(255, 0, 8);

static void fat_text(Mat &screen, int x, int y, const char *text)
{
	putText(screen, text, Point(x, y), font, font_scale, blue, thickness);
}

static void fat_text_cent(Mat &screen, int x, int y, const char *text)
{
	int baseline;

	Size size = getTextSize(text, font, font_scale, thickness, &baseline);
	putText(screen, text, Point(x - size.width / 2, y - size.height /2),
		font, font_scale, blue, thickness);
}

static void draw_qr(Mat &screen, struct quirc *q, struct dthash *dt)
{
	int count = quirc_count(q);
	int i;

	for (i = 0; i < count; i++) {
		struct quirc_code code;
		struct quirc_data data;
		quirc_decode_error_t err;
		int j;
		int xc = 0;
		int yc = 0;
		char buf[128];

		quirc_extract(q, i, &code);

		for (j = 0; j < 4; j++) {
			struct quirc_point *a = &code.corners[j];
			struct quirc_point *b = &code.corners[(j + 1) % 4];

			xc += a->x;
			yc += a->y;
			line(screen, Point(a->x, a->y), Point(b->x, b->y), blue, 8);
		}

		xc /= 4;
		yc /= 4;

		if (want_verbose) {
			snprintf(buf, sizeof(buf), "Code size: %d cells",
				 code.size);
			fat_text_cent(screen, xc, yc - 20, buf);
		}

		err = quirc_decode(&code, &data);

		if (err) {
			if (want_verbose)
				fat_text_cent(screen, xc, yc, quirc_strerror(err));
		} else {
			fat_text_cent(screen, xc, yc, (const char *)data.payload);
			print_data(&data, dt, want_verbose);

			if (want_verbose) {
				snprintf(buf, sizeof(buf),
					 "Ver: %d, ECC: %c, Mask: %d, Type: %d",
					 data.version, "MLHQ"[data.ecc_level],
					 data.mask, data.data_type);
				fat_text_cent(screen, xc, yc + 20, buf);
			}
		}
	}
}

static int main_loop(VideoCapture &cap, struct quirc *q)
{
	time_t last_rate = 0;
	int frame_count = 0;
	char rate_text[64];
	struct dthash dt;

	rate_text[0] = 0;
	dthash_init(&dt, printer_timeout);

	Mat frame;
	for (;;) {
		time_t now = time(NULL);

		cap.read(frame);
		if (frame.empty()) {
			perror("empty frame");
			return 0;
		}

		int w;
		int h;
		uint8_t *buf = quirc_begin(q, &w, &h);

		/* convert frame into buf */
		assert(frame.cols == w);
		assert(frame.rows == h);
		Mat gray;
		cvtColor(frame, gray, COLOR_BGR2GRAY, 0);
		for (int y = 0; y < gray.rows; y++) {
			for (int x = 0; x < gray.cols; x++) {
				buf[(y * w + x)] = gray.at<uint8_t>(y, x);
			}
		}

		quirc_end(q);

		draw_qr(frame, q, &dt);
		if (want_frame_rate)
			fat_text(frame, 20, 20, rate_text);

		imshow("quirc-demo-opencv", frame);
		waitKey(5);

		if (now != last_rate) {
			snprintf(rate_text, sizeof(rate_text),
				 "Frame rate: %d fps", frame_count);
			frame_count = 0;
			last_rate = now;
		}

		frame_count++;
	}
}

static int run_demo(void)
{
	struct quirc *qr;
	VideoCapture cap(0);
	unsigned int width;
	unsigned int height;

	if (!cap.isOpened()) {
		perror("camera_open");
		goto fail_qr;
	}

	width = cap.get(CAP_PROP_FRAME_WIDTH);
	height = cap.get(CAP_PROP_FRAME_HEIGHT);

	qr = quirc_new();
	if (!qr) {
		perror("couldn't allocate QR decoder");
		goto fail_qr;
	}

	if (quirc_resize(qr, width, height) < 0) {
		perror("couldn't allocate QR buffer");
		goto fail_qr_resize;
	}

	if (main_loop(cap, qr) < 0) {
		goto fail_main_loop;
	}

	quirc_destroy(qr);

	return 0;

fail_main_loop:
fail_qr_resize:
	quirc_destroy(qr);
fail_qr:

	return -1;
}

static void usage(const char *progname)
{
	printf("Usage: %s [options]\n\n"
"Valid options are:\n\n"
"    -f             Show frame rate on screen.\n"
"    -v             Show extra data for detected codes.\n"
"    -p <timeout>   Set printer timeout (seconds).\n"
"    --help         Show this information.\n"
"    --version      Show library version information.\n",
	progname);
}

int main(int argc, char **argv)
{
	static const struct option longopts[] = {
		{"help",		0, 0, 'H'},
		{"version",		0, 0, 'V'},
		{NULL,			0, 0, 0}
	};
	int opt;

	printf("quirc demo with OpenCV\n");
	printf("Copyright (C) 2010-2014 Daniel Beer <dlbeer@gmail.com>\n");
	printf("\n");

	while ((opt = getopt_long(argc, argv, "fvg:p:",
				  longopts, NULL)) >= 0)
		switch (opt) {
		case 'V':
			printf("Library version: %s\n", quirc_version());
			return 0;

		case 'H':
			usage(argv[0]);
			return 0;

		case 'v':
			want_verbose = 1;
			break;

		case 'f':
			want_frame_rate = 1;
			break;

		case 'p':
			printer_timeout = atoi(optarg);
			break;

		case '?':
			fprintf(stderr, "Try --help for usage information\n");
			return -1;
		}

	return run_demo();
}
