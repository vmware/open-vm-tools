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
   
   /*
    * 'depth' is the color depth (in bits per pixel) used for the image. 'bpp'
    * is the number of bits actually consumed per pixel in memory. (For
    * example, an image that uses 5 bits for each of R, G, B has depth=15 and
    * bpp=16.  If an image has an alpha channel, the alpha bits are counted in
    * 'bpp' but not in 'depth'.) It's always true that depth <= bpp.
    * 
    * Also see the comment to Raster_ConvertPixels.
    */
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
   int zlibCompressLevel; // Set to -1 for the default compression level.
   Bool stripAlphaChannel;
} ImagePngWriteOptions;

typedef enum {
   IMAGE_PNG_READ_KEEP_ALPHA = (1 << 0),
} ImagePngReadFlags;

#endif // _IMAGEUTIL_TYPES_H_
