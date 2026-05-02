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

#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <quirc.h>
#include <time.h>
#include <getopt.h>

#include "camera.h"
#include "mjpeg.h"
#include "convert.h"
#include "dthash.h"
#include "demoutil.h"

/* Collected command-line arguments */
static const char *camera_path = "/dev/video0";
static int video_width = 640;
static int video_height = 480;
static int want_verbose = 0;
static int printer_timeout = 2;

static int main_loop(struct camera *cam,
		     struct quirc *q, struct mjpeg_decoder *mj)
{
	struct dthash dt;

	dthash_init(&dt, printer_timeout);

	for (;;) {
		int w, h;
		int i, count;
		uint8_t *buf = quirc_begin(q, &w, &h);
		const struct camera_buffer *head;
		const struct camera_parms *parms = camera_get_parms(cam);

		if (camera_dequeue_one(cam) < 0) {
			perror("camera_dequeue_one");
			return -1;
		}

		head = camera_get_head(cam);

		switch (parms->format) {
		case CAMERA_FORMAT_MJPEG:
			mjpeg_decode_gray(mj, head->addr, head->size,
					  buf, w, w, h);
			break;

		case CAMERA_FORMAT_YUYV:
			yuyv_to_luma(head->addr, w * 2, w, h, buf, w);
			break;

		default:
			fprintf(stderr, "Unknown frame format\n");
			return -1;
		}

		if (camera_enqueue_all(cam) < 0) {
			perror("camera_enqueue_all");
			return -1;
		}

		quirc_end(q);

		count = quirc_count(q);
		for (i = 0; i < count; i++) {
			struct quirc_code code;
			struct quirc_data data;

			quirc_extract(q, i, &code);
			if (!quirc_decode(&code, &data))
				print_data(&data, &dt, want_verbose);
		}
	}
}

static int run_scanner(void)
{
	struct quirc *qr;
	struct camera cam;
	struct mjpeg_decoder mj;
	const struct camera_parms *parms;

	camera_init(&cam);
	if (camera_open(&cam, camera_path, video_width, video_height,
			25, 1) < 0) {
		perror("camera_open");
		goto fail_qr;
	}

	if (camera_map(&cam, 8) < 0) {
		perror("camera_map");
		goto fail_qr;
	}

	if (camera_on(&cam) < 0) {
		perror("camera_on");
		goto fail_qr;
	}

	if (camera_enqueue_all(&cam) < 0) {
		perror("camera_enqueue_all");
		goto fail_qr;
	}

	parms = camera_get_parms(&cam);

	qr = quirc_new();
	if (!qr) {
		perror("couldn't allocate QR decoder");
		goto fail_qr;
	}

	if (quirc_resize(qr, parms->width, parms->height) < 0) {
		perror("couldn't allocate QR buffer");
		goto fail_qr_resize;
	}

	mjpeg_init(&mj);
	if (main_loop(&cam, qr, &mj) < 0)
		goto fail_main_loop;
	mjpeg_free(&mj);

	quirc_destroy(qr);
	camera_destroy(&cam);

	return 0;

fail_main_loop:
	mjpeg_free(&mj);
fail_qr_resize:
	quirc_destroy(qr);
fail_qr:
	camera_destroy(&cam);

	return -1;
}

static void usage(const char *progname)
{
	printf("Usage: %s [options]\n\n"
"Valid options are:\n\n"
"    -v             Show extra data for detected codes.\n"
"    -d <device>    Specify camera device path.\n"
"    -s <WxH>       Specify video dimensions.\n"
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

	printf("quirc scanner demo\n");
	printf("Copyright (C) 2010-2012 Daniel Beer <dlbeer@gmail.com>\n");
	printf("\n");

	while ((opt = getopt_long(argc, argv, "d:s:vg:p:",
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

		case 's':
			if (parse_size(optarg, &video_width, &video_height) < 0)
				return -1;
			break;

		case 'p':
			printer_timeout = atoi(optarg);
			break;

		case 'd':
			camera_path = optarg;
			break;

		case '?':
			fprintf(stderr, "Try --help for usage information\n");
			return -1;
		}

	return run_scanner();
}
