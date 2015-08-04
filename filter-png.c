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

#include <sysexits.h>
#include <string.h>
#include <png.h>
#include <zlib.h>
#include <assert.h>

#include "capture.h"

int filter_png(struct media_info const *info, int out_fd, int in_fd)
{
	FILE			*fp = fdopen(out_fd, "wb");
	struct png_struct_def	*png_ptr = NULL;
	struct png_info_def	*png_info = NULL;
	volatile int		rc = EX_SOFTWARE;
	png_bytep		row_pointers[info->height];
	void * volatile		raw_data = NULL;
	void * volatile		rgb_data = NULL;
	size_t			rgb_stride =
		info->width * ((info->bpp + 7) / 8) * 3;

	if (!fp) {
		perror("fdopen()");
		return EX_OSERR;
	}

	png_ptr =  png_create_write_struct(PNG_LIBPNG_VER_STRING,
					   NULL, NULL, NULL);
	if (!png_ptr)
		goto out;

	if (setjmp(png_jmpbuf(png_ptr)))
		goto out;

	/* allocate memory for raw (YCbCr) image data */
	raw_data = png_malloc(png_ptr, info->stride * info->height);
	if (!raw_data)
		goto out;

	rgb_data = png_malloc(png_ptr, rgb_stride * info->height);
	if (!rgb_data)
		goto out;

	for (size_t y = 0; y < info->height; ++y)
		row_pointers[y] = rgb_data + y * rgb_stride;

	png_info = png_create_info_struct(png_ptr);
	if (!png_info)
		goto out;

	png_init_io(png_ptr, fp);
	png_set_user_limits(png_ptr, info->width, info->height);
	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);

	/* atm, only 8bpp support is implemented */
	assert(info->bpp == 8);

	for (;;) {
		bool	eof = false;

		if (read_all(in_fd, raw_data, info->stride * info->height, &eof))
			;		/* noop */
		else if (!eof)
			goto out;
		else
			break;

		convert_yuv422_rgb888(rgb_data, raw_data, info->bpp,
				      info->width, info->height);

		png_set_rows(png_ptr, png_info, row_pointers);

		png_set_IHDR(png_ptr, png_info, info->width, info->height,
			     info->bpp, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			     PNG_COMPRESSION_TYPE_DEFAULT,
			     PNG_FILTER_TYPE_DEFAULT);
		png_write_png(png_ptr, png_info, PNG_TRANSFORM_IDENTITY, NULL);
		fflush(fp);
		break;
	}

	rc = 0;

out:
	png_free(png_ptr, rgb_data);
	png_free(png_ptr, raw_data);
	png_destroy_write_struct(&png_ptr, &png_info);
	fclose(fp);

	return rc;
}
