/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * rasterConv.h --
 *
 *	Pixel conversion routines
 */

#ifndef _RASTER_CONV_H_
#define _RASTER_CONV_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

/*
 * To minimize the amount of code needed to support the rarely-useful
 * BGR111 and BGR222 modes, their masks are defined to match the
 * appropriate number of high bits of the BGR233 masks.
 */

#define REDMASK_BGR111		0x04
#define	GREENMASK_BGR111	0x20
#define	BLUEMASK_BGR111		0x80

#define REDMASK_RGB222		0x30
#define	GREENMASK_RGB222	0x0c
#define	BLUEMASK_RGB222		0x03 

#define REDMASK_BGR222		0x03
#define	GREENMASK_BGR222	0x0c
#define	BLUEMASK_BGR222		0x30

#define REDMASK_BGR233		0x07
#define	GREENMASK_BGR233	0x38
#define	BLUEMASK_BGR233		0xc0

#define REDMASK_15		0x7c00
#define	GREENMASK_15		0x03e0
#define	BLUEMASK_15		0x001f

#define	GREEN_HIBIT_15		0x0200
#define GREEN_HILOSHIFT_15	4

#define REDMASK_16		0xf800
#define	GREENMASK_16		0x07e0
#define	BLUEMASK_16		0x001f

#define REDMASK_24		0x00ff0000
#define	GREENMASK_24		0x0000ff00
#define	BLUEMASK_24		0x000000ff

#define REDMASK_32		0x00ff0000
#define	GREENMASK_32		0x0000ff00
#define	BLUEMASK_32		0x000000ff

Bool Raster_IsModeReasonable(uint32 depth, uint32 bpp, Bool pseudocolor);

int Raster_GetBPPDepth(uint32 depth, uint32 bpp);

void Raster_ConvertPixels(uint8 *tof, int32 line_increment, int bppdepth,
			  const uint8 *src, int32 src_increment, int src_bppdepth,
			  Bool pseudoColor, const uint32 *pixels,
			  uint32 src_x, uint32 src_y,
			  uint32 x, uint32 y, uint32 w, uint32 h);

uint32 Raster_ConvertOnePixel(uint32 pixel, int src_depth, int depth,
			      Bool pseudoColor, const uint32 *pixels);

Bool Raster_ConversionParameters(int depth, uint32 *redMask,
				 uint32 *greenMask, uint32 *blueMask);

#endif // _RASTER_CONV_H_
