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

#include "yuv422rgb888.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/param.h>

#include "capture.h"

#define SCALE(_x)	((_x) * 0x10000)
#define SCALE1(_x)	((SCALE(_x) / 256))
#define UNSCALE(_x)	((_x) / 0x10000)
#define ROW(a, b, c)	{ (a), (b), (c) }

static int32_t const	MATRIX[3][4] = { RGB_MATRIX };

inline static uint8_t *_cvt(uint8_t *dst,
			    signed int y, signed int cr, signed int cb)
{
	static bool	first = true;
#if 1
	int32_t	v[3] = {
		y * MATRIX[0][0] + cr * MATRIX[0][1] + cb * MATRIX[0][2],
		y * MATRIX[1][0] + cr * MATRIX[1][1] + cb * MATRIX[1][2],
		y * MATRIX[2][0] + cr * MATRIX[2][1] + cb * MATRIX[2][2],
	};

	*dst++ = MIN(MAX(UNSCALE(v[0]), 0), 255);
	*dst++ = MIN(MAX(UNSCALE(v[1]), 0), 255);
	*dst++ = MIN(MAX(UNSCALE(v[2]), 0), 255);

	if (0 && first) {
		printf("[%d %d %d | %d %d %d | %d %d %d ] * [ %u %d %d ] | [%d %d %d] -> [%d %d %d]\n",
		       MATRIX[0][0], MATRIX[0][1], MATRIX[0][2],
		       MATRIX[1][0], MATRIX[1][1], MATRIX[1][2],
		       MATRIX[2][0], MATRIX[2][1], MATRIX[2][2],
		       y, cr, cb,
		       v[0], v[1], v[2],
		       dst[-3], dst[-2], dst[-1]);
		first = false;
	}
#else

	/* from http://en.wikipedia.org/wiki/YUV#Y.27UV444_to_RGB888_conversion */
	int	r = y + cr + (cr >> 2) + (cr >> 3) + (cr >> 5);
	int	g = (y
		     - ((cb >> 2) + (cb >> 4) + (cb >> 5))
		     - ((cr >> 1) + (cr >> 3) + (cr >> 4) + (cr >> 5)));
	int	b = y + cb + (cb >> 1) + (cb >> 2) + (cb >> 6);

	*dst++ = MIN(r, 255);
	*dst++ = MIN(g, 255);
	*dst++ = MIN(b, 255);
#endif

	return dst;
}

static void convert_yuv422_rgb888_8(uint8_t *dst, uint8_t const *src,
				    unsigned int width, unsigned int height)
{
	unsigned int		y;

	for (y = 0; y < height; ++y) {
		unsigned int		x;
		for (x = 0; x+1 < width; x += 2) {
			unsigned int	y0 = *src++;
			unsigned int	cr = *src++;
			unsigned int	y1 = *src++;
			unsigned int	cb = *src++;

			dst = _cvt(dst, y0 - 16, cr - 128, cb - 128);
			dst = _cvt(dst, y1 - 16, cr - 128, cb - 128);
		}
	}
}

void convert_yuv422_rgb888(void *dst, void const *src, int bpp,
			   unsigned int width, unsigned int height)
{
	if (bpp == 8)
		convert_yuv422_rgb888_8(dst, src, width, height);
	else
		abort();
}
