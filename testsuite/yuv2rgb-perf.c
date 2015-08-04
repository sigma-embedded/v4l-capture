/*	--*- c -*--
 * Copyright (C) 2014 Enrico Scholz <enrico.scholz@ensc.de>
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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "../capture.h"

int main(int argc, char *argv[])
{
	unsigned int		w   = atoi(argv[1]);
	unsigned int		h   = atoi(argv[2]);
	unsigned int		bpp = atoi(argv[3]);
	unsigned int		cnt = atoi(argv[4]);
	size_t			stride = w * ((bpp + 7) / 8) * 2;

	void			*src;
	void			*dst;

	struct timespec		tm_a;
	struct timespec		tm_b;
	uint64_t		tm_delta;


	src = malloc(stride * h);
	assert(src != 0);

	dst = malloc(stride * 3 / 2 * h);
	assert(dst != 0);

	read_all(0, src, stride * h, NULL);

	clock_gettime(CLOCK_MONOTONIC_RAW, &tm_a);
	while (cnt-- > 0)
		convert_yuv422_rgb888(dst, src, bpp, w, h);
	clock_gettime(CLOCK_MONOTONIC_RAW, &tm_b);

	write(3, dst, stride * 3 / 2 * h);

	tm_delta  = 1000000000LLu * tm_b.tv_sec + tm_b.tv_nsec;
	tm_delta -= 1000000000LLu * tm_a.tv_sec + tm_a.tv_nsec;

	printf("tm_delta=%lu.%06lu\n",
	       (unsigned long)(tm_delta / 1000000000Lu),
	       (unsigned long)(tm_delta / 1000Lu % 1000000));
}
