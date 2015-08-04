/*	--*- c -*--
 * Copyright (C) 2014 Enrico Scholz <enrico.scholz@sigma-chemnitz.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "../capture.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

int main(int argc, char *argv[])
{
	struct media_info	info = {
		.width	= atoi(argv[2]),
		.height = atoi(argv[3]),
		.bpp	= atoi(argv[4]),
		.stride = atoi(argv[5]),
	};

	struct timespec		tm_a;
	struct timespec		tm_b;
	uint64_t		tm_delta;

	clock_gettime(CLOCK_MONOTONIC_RAW, &tm_a);
	if (strcmp(argv[1], "png") == 0)
		filter_png(&info, 3, 0);
	else if (strcmp(argv[1], "jpg") == 0)
		filter_jpg(&info, 3, 0);
	else
		abort();
	clock_gettime(CLOCK_MONOTONIC_RAW, &tm_b);

	tm_delta  = 1000000000LLu * tm_b.tv_sec + tm_b.tv_nsec;
	tm_delta -= 1000000000LLu * tm_a.tv_sec + tm_a.tv_nsec;

	printf("tm_delta=%lu.%06lu\n",
	       (unsigned long)(tm_delta / 1000000000Lu),
	       (unsigned long)(tm_delta / 1000Lu % 1000000));
}
