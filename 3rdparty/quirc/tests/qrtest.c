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
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <quirc.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <time.h>
#include "dbgutil.h"

static int want_verbose = 0;
static int want_cell_dump = 0;

#define MS(ts) (unsigned int)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000))

static struct quirc *decoder;

struct result_info {
	int		file_count;
	int		id_count;
	int		decode_count;

	unsigned int	load_time;
	unsigned int	identify_time;
	unsigned int	total_time;
};

static void print_result(const char *name, struct result_info *info)
{
	puts("----------------------------------------"
	     "---------------------------------------");
	printf("%s: %d files, %d codes, %d decoded (%d failures)",
	       name, info->file_count, info->id_count, info->decode_count,
	       (info->id_count - info->decode_count));
	if (info->id_count)
		printf(", %d%% success rate",
		       (info->decode_count * 100 + info->id_count / 2) /
			info->id_count);
	printf("\n");
	printf("Total time [load: %u, identify: %u, total: %u]\n",
	       info->load_time,
	       info->identify_time,
	       info->total_time);
	if (info->file_count)
		printf("Average time [load: %u, identify: %u, total: %u]\n",
		       info->load_time / info->file_count,
		       info->identify_time / info->file_count,
		       info->total_time / info->file_count);
}

static void add_result(struct result_info *sum, struct result_info *inf)
{
	sum->file_count += inf->file_count;
	sum->id_count += inf->id_count;
	sum->decode_count += inf->decode_count;

	sum->load_time += inf->load_time;
	sum->identify_time += inf->identify_time;
	sum->total_time += inf->total_time;
}

static int scan_file(const char *path, const char *filename,
		     struct result_info *info)
{
	int (*loader)(struct quirc *, const char *);
	int len = strlen(filename);
	const char *ext;
	struct timespec tp;
	unsigned int start;
	unsigned int total_start;
	int ret;
	int i;

	while (len >= 0 && filename[len] != '.')
		len--;
	ext = filename + len + 1;
	if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0)
		loader = load_jpeg;
	else if (strcasecmp(ext, "png") == 0)
		loader = load_png;
	else
		return 0;

	(void)clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp);
	total_start = start = MS(tp);
	ret = loader(decoder, path);
	(void)clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp);
	info->load_time = MS(tp) - start;

	if (ret < 0) {
		fprintf(stderr, "%s: load failed\n", filename);
		return -1;
	}

	(void)clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp);
	start = MS(tp);
	quirc_end(decoder);
	(void)clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp);
	info->identify_time = MS(tp) - start;

	info->id_count = quirc_count(decoder);
	for (i = 0; i < info->id_count; i++) {
		struct quirc_code code;
		struct quirc_data data;

		quirc_extract(decoder, i, &code);

		quirc_decode_error_t err = quirc_decode(&code, &data);
		if (err == QUIRC_ERROR_DATA_ECC) {
			quirc_flip(&code);
			err = quirc_decode(&code, &data);
		}

		if (!err) {
			info->decode_count++;
		}
	}

	(void)clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp);
	info->total_time += MS(tp) - total_start;

	printf("  %-30s: %5u %5u %5u %5d %5d\n", filename,
	       info->load_time,
	       info->identify_time,
	       info->total_time,
	       info->id_count, info->decode_count);

	if (want_cell_dump || want_verbose) {
		for (i = 0; i < info->id_count; i++) {
			struct quirc_code code;

			quirc_extract(decoder, i, &code);
			if (want_cell_dump) {
				dump_cells(&code);
				printf("\n");
			}

			if (want_verbose) {
				struct quirc_data data;
				quirc_decode_error_t err = quirc_decode(&code, &data);
				if (err == QUIRC_ERROR_DATA_ECC) {
					quirc_flip(&code);
					err = quirc_decode(&code, &data);
				}

				if (err) {
					printf("  ERROR: %s\n\n", quirc_strerror(err));
				} else {
					printf("  Decode successful:\n");
					dump_data(&data);
					printf("\n");
				}
			}
		}
	}

	info->file_count = 1;
	return 1;
}

static int test_scan(const char *path, struct result_info *info);

static int scan_dir(const char *path, const char *filename,
		    struct result_info *info)
{
	DIR *d = opendir(path);
	struct dirent *ent;
	int count = 0;

	if (!d) {
		fprintf(stderr, "%s: opendir: %s\n", path, strerror(errno));
		return -1;
	}

	printf("%s:\n", path);

	while ((ent = readdir(d))) {
		if (ent->d_name[0] != '.') {
			char fullpath[1024];
			struct result_info sub;

			snprintf(fullpath, sizeof(fullpath), "%s/%s",
				 path, ent->d_name);
			if (test_scan(fullpath, &sub) > 0) {
				add_result(info, &sub);
				count++;
			}
		}
	}

	closedir(d);

	if (count > 1) {
		print_result(filename, info);
		puts("");
	}

	return count > 0;
}

static int test_scan(const char *path, struct result_info *info)
{
	int len = strlen(path);
	struct stat st;
	const char *filename;

	memset(info, 0, sizeof(*info));

	while (len >= 0 && path[len] != '/')
		len--;
	filename = path + len + 1;

	if (lstat(path, &st) < 0) {
		fprintf(stderr, "%s: lstat: %s\n", path, strerror(errno));
		return -1;
	}

	if (S_ISREG(st.st_mode))
		return scan_file(path, filename, info);

	if (S_ISDIR(st.st_mode))
		return scan_dir(path, filename, info);

	return 0;
}

static int run_tests(int argc, char **argv)
{
	struct result_info sum;
	int count = 0;
	int i;

	decoder = quirc_new();
	if (!decoder) {
		perror("quirc_new");
		return -1;
	}

	printf("  %-30s  %17s %11s\n", "", "Time (ms)", "Count");
	printf("  %-30s  %5s %5s %5s %5s %5s\n",
	       "Filename", "Load", "ID", "Total", "ID", "Dec");
	puts("----------------------------------------"
	     "---------------------------------------");

	memset(&sum, 0, sizeof(sum));
	for (i = 0; i < argc; i++) {
		struct result_info info;

		if (test_scan(argv[i], &info) > 0) {
			add_result(&sum, &info);
			count++;
		}
	}

	if (count > 1)
		print_result("TOTAL", &sum);

	quirc_destroy(decoder);
	return 0;
}

int main(int argc, char **argv)
{
	int opt;

	printf("quirc test program\n");
	printf("Copyright (C) 2010-2012 Daniel Beer <dlbeer@gmail.com>\n");
	printf("Library version: %s\n", quirc_version());
	printf("\n");

	while ((opt = getopt(argc, argv, "vd")) >= 0)
		switch (opt) {
		case 'v':
			want_verbose = 1;
			break;

		case 'd':
			want_cell_dump = 1;
			break;

		case '?':
			return -1;
		}

	argv += optind;
	argc -= optind;

	return run_tests(argc, argv);;
}
