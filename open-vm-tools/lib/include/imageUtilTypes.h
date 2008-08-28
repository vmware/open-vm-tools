/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
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
 * imageUtilTypes.h --
 *
 */

#ifndef _IMAGEUTIL_TYPES_H_
#define _IMAGEUTIL_TYPES_H_

typedef enum {
   IMAGE_TYPE_PNG,
   IMAGE_TYPE_BMP
} ImageType;

typedef enum {
   IMAGE_FLAG_TOP_DOWN  = 0,         /* Top-down scanlines are the default */
   IMAGE_FLAG_BOTTOM_UP = (1 << 0),  /* Bottom-up storage (BMP or OpenGL style) */
} ImageFlags;

typedef struct {
   unsigned char blue;
   unsigned char green;
   unsigned char red;
   unsigned char reserved;
} ImageColor;

typedef struct {
   unsigned int width;
   unsigned int height;
   unsigned int depth;
   unsigned int bpp;
   unsigned int bytesPerLine;
   ImageFlags flags;
   union {
      struct {
         unsigned int numColors;
         ImageColor palette[256];
      };
      struct {
         unsigned int redMask;
         unsigned int blueMask;
         unsigned int greenMask;
      };
   };
   unsigned char *data;
} ImageInfo;

typedef struct {
   int zlibCompressLevel;
   Bool stripAlphaChannel;
} ImagePngOptions;

#endif // _IMAGEUTIL_TYPES_H_
