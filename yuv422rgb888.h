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

#ifndef H_TQ_TVW_CAPTURE_YUV422RGB888_H
#define H_TQ_TVW_CAPTURE_YUV422RGB888_H

#define BT601_MATRIX \
	ROW(SCALE(1), SCALE(0),        SCALE(1.28033)),	 \
	ROW(SCALE(1), SCALE(-0.21482), SCALE(0.38059)), \
	ROW(SCALE(1), SCALE(-2.12798), SCALE(0)),

#define YCBCR_MATRIX \
	ROW(SCALE(0.00456621), SCALE(0),           SCALE( 0.00625893)), \
	ROW(SCALE(0.00456621), SCALE(-0.00153632), SCALE(-0.00318811)), \
	ROW(SCALE(0.00456621), SCALE( 0.00791071), SCALE(0))

/* http://www.poynton.com/notes/colour_and_gamma/ColorFAQ.html#RTFToC30 */
#define RGB_MATRIX \
	ROW(SCALE1(298.082), SCALE1(0),        SCALE1(408.583)), \
	ROW(SCALE1(298.082), SCALE1(-100.291), SCALE1(-208.120)), \
	ROW(SCALE1(298.082), SCALE1( 516.411), SCALE1(0))

#endif	/* H_TQ_TVW_CAPTURE_YUV422RGB888_H */
