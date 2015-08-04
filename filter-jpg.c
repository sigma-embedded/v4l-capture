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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sysexits.h>
#include <jpeglib.h>

#include "capture.h"

int filter_jpg(struct media_info const *info, int out_fd, int in_fd)
{
	FILE				*fp;
	int const			quality = 93; /* TODO: removed
						       * hardcoded value */
	struct jpeg_compress_struct	cinfo;
	struct jpeg_error_mgr		jerr;

	fp = fdopen(out_fd, "wb");
	if (!fp) {
		perror("fdopen()");
		return EX_OSERR;
	}

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, fp);

	cinfo.image_width = info->width;
	cinfo.image_height = info->height;
	cinfo.input_components = 3;	  /* TODO: really? */
	cinfo.in_color_space = JCS_YCbCr; /* TODO: calculate it from info */
	cinfo.data_precision = info->bpp;

	jpeg_set_defaults(&cinfo);
	jpeg_set_colorspace(&cinfo, JCS_YCbCr);
	jpeg_set_quality(&cinfo, quality, TRUE);

	cinfo.comp_info[0].h_samp_factor = 2;
	cinfo.comp_info[0].v_samp_factor = 1;
	cinfo.comp_info[1].h_samp_factor = 1;
	cinfo.comp_info[1].v_samp_factor = 1;
	cinfo.comp_info[2].h_samp_factor = 1;
	cinfo.comp_info[2].v_samp_factor = 1;

	for (;;) {
		char		row_buf[info->stride];
		size_t		y;
		bool		eof = false;

		jpeg_start_compress(&cinfo, TRUE);

		for (y = 0; y < info->height; ++y) {
			if (read_all(in_fd, row_buf, info->stride, &eof))
				;	/* noop */
			else if (!eof)
				return EX_SOFTWARE;
			else
				break;

			if (info->bpp <= 8) {
				typedef uint8_t		pix_t;
#include "filter-jpg_samp.inc.h"
			} else if (info->bpp <= 16) {
				typedef uint16_t	pix_t;
#include "filter-jpg_samp.inc.h"
			} else {
				abort();
			}
		}

		if (!eof)
			jpeg_finish_compress(&cinfo);
		else
			break;

		fflush(fp);
	}

	fclose(fp);
	jpeg_destroy_compress(&cinfo);

	return 0;
}
