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

#include "capture.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/param.h>
#include <arm_neon.h>

#include "yuv422rgb888.h"

#define SCALE(_x)	((_x) * 0x10000)
#define SCALE1(_x)	((SCALE(_x) / 256))
#define UNSCALE(_x)	((_x) / 0x10000)
#define ROW(a, b, c)	{ (a), (b), (c), 0 }

static int32x4_t const	MATRIX_REV[3] = {
	{  76308,  76308,  76308, 0 },
	{      0, -25674, 132201, 0 },
	{ 104597, -53278,      0, 0 },
};

static int32x4_t const	MATRIX_DELTA = {
	 -14609344, 8884928, -18142656,
 };

#if 0
static void fill_matrix_rev(void) {
	static bool	filled = false;

	size_t		i;

	if (filled)
		return;

	for (i = 0; i < 3; ++i) {
		size_t	j;

		for (j = 0; j < 3; ++j)
			MATRIX_REV[i][j] = MATRIX_ROW[j][i];

		MATRIX_REV[i][3] = 0;
	}

	MATRIX_DELTA  = vmulq_n_s32(MATRIX_REV[0], -16);
	MATRIX_DELTA += vmulq_n_s32(MATRIX_REV[1], -128);
	MATRIX_DELTA += vmulq_n_s32(MATRIX_REV[2], -128);

	filled = true;

	for (i = 0; 1 && i < 3; ++i)
		printf("%10d | %10d %10d %10d\n",
		       (int)MATRIX_DELTA[i],
		       (int)MATRIX_REV[i][0], (int)MATRIX_REV[i][1],
		       (int)MATRIX_REV[i][2]);
}
#endif

static void convert_yuv422_rgb888_8(uint8_t *dst, uint8_t const *src,
				    unsigned int width, unsigned int height)
{
	unsigned int		p;

	for (p = width * height / 2; p > 0; --p) {
		uint8_t		y0 = *src++;
		uint8_t		cr = *src++;
		uint8_t		y1 = *src++;
		uint8_t		cb = *src++;

		int32x4_t	s_cr = vmlaq_n_s32(MATRIX_DELTA,
						   MATRIX_REV[1], cr);
		int32x4_t	s_c  = vmlaq_n_s32(s_cr, MATRIX_REV[2], cb);
		int32x4_t	rgb0 = vmlaq_n_s32(s_c,	 MATRIX_REV[0], y0);
		int32x4_t	rgb1 = vmlaq_n_s32(s_c,	 MATRIX_REV[0], y1);

		uint16x4_t	rgb0_16 = vqshrun_n_s32(rgb0, 16);
		uint16x4_t	rgb1_16 = vqshrun_n_s32(rgb1, 16);

		uint16x8_t	rgb16 = vcombine_u16(rgb0_16, rgb1_16);
		uint8x8_t	rgb8  = vqmovn_u16(rgb16);

		*dst++ = rgb8[0];
		*dst++ = rgb8[1];
		*dst++ = rgb8[2];

		*dst++ = rgb8[4];
		*dst++ = rgb8[5];
		*dst++ = rgb8[6];

		if (0) {
			static bool	first = true;

			if (first) {
				printf("%u %u %u %u | %u %u %u | %u %u %u\n",
				       y0, cr, y1, cb,
				       dst[-6], dst[-5], dst[-4],
				       dst[-3], dst[-2], dst[-1]);
				first = false;
			}
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
