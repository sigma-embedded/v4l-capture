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

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "capture.h"

bool read_all(int fd, void *dst, size_t len, bool *eof)
{
	while (len > 0) {
		ssize_t	l = read(fd, dst, len);

		if (l > 0) {
			dst += l;
			len -= l;
		} else if (l == 0) {
			if (eof)
				*eof = true;
			else
				fprintf(stderr, "read_all: read(): EOF\n");
			break;
		} else if (errno == EINTR) {
			continue;
		} else {
			fprintf(stderr, "read_all: read(): %s\n",
				strerror(errno));
			break;
		}
	}

	return len == 0;
}
