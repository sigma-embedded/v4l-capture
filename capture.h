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

#ifndef H_TQ_TVW_CAPTURE_H
#define H_TQ_TVW_CAPTURE_H

#include <unistd.h>
#include <stdbool.h>

enum media_stream_fmt {
	OUTPUT_FMT_GST0,
	OUTPUT_FMT_GST1,
	OUTPUT_FMT_RAW,
};

struct media_info {
	int			fd_ipu_dev;
	int			fd_video_dev;
	int			fd_sensor_dev;

	char const		*name_ipu_entity;
	char const		*name_video_entity;
	char const		*name_sensor_entity;

	unsigned int		width;
	unsigned int		height;
	unsigned int		bpp;
	unsigned int		stride;
	unsigned int		size;
	unsigned int		rate;
	unsigned int		cam_rate;
	unsigned int		fourcc;

	char const		*gst_cap;
	enum media_stream_fmt	out_fmt;

	pid_t			pid_filter;
};

typedef int	(*filter_run_fn)(struct media_info const *info, int out_fd,
				 int in_fd);

int	filter_png(struct media_info const *info, int out_fd, int in_fd);
int	filter_jpg(struct media_info const *info, int out_fd, int in_fd);

bool	read_all(int fd, void *dst, size_t l, bool *eof);

void	convert_yuv422_rgb888(void *dst, void const *src, int bpp,
			      unsigned int width, unsigned int height);

#endif	/* H_TQ_TVW_CAPTURE_H */
