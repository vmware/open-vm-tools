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
 * imageUtil.h --
 *
 */

#ifndef _IMAGEUTIL_H_
#define _IMAGEUTIL_H_

#include "imageUtilTypes.h"
#include "dynbuf.h"
#include "unicodeTypes.h"

#ifdef _WIN32

typedef char Bool;

typedef struct tagDIBINFOHEADER {
   BITMAPINFOHEADER  bmiHeader;
   RGBQUAD           bmiColors[256];
   LPBYTE            bitmapBits;
} DIBINFOHEADER, FAR *LPDIBINFOHEADER, *PDIBINFOHEADER;

BOOL ImageUtil_SavePNG(LPDIBINFOHEADER image, ConstUnicode fileName);
EXTERN HBITMAP ImageUtil_LoadPNG(ConstUnicode fileName, int pngReadFlags);
EXTERN HBITMAP ImageUtil_LoadPNGFromBuffer(const unsigned char *imageData,
                                           unsigned int dataLen, int pngReadFlags);
EXTERN HBITMAP ImageUtil_LoadImage(ConstUnicode filename, unsigned int width,
                                                          unsigned int height);

#else
#ifndef NO_X11_XLIB_H    /* XXX Carbon and X both define 'Cursor'
                          * but we don't use ImgUtil_SavePNG in mksQuartz.h
                          * Remove after we have implemeted imageUtil with
                          * Quartz.
                          */
#include <X11/Xlib.h>
#undef Bool                            // XXX conflict with vm_basic_types.h
#define XBool int
#undef Cursor

Bool ImageUtil_SavePNG(const XImage *image,
                       unsigned int numColors, unsigned short colors[256][3],
                       const char *fileName);
#endif //USE_IMAGE_UTIL_H
#endif

#define DWORD_ALIGN(x)             ((((x)+3) >> 2) << 2)

EXTERN Bool ImageUtil_ReadPNG(ImageInfo *image,
                              ConstUnicode pathName,
                              int pngReadFlags);

EXTERN Bool ImageUtil_WriteImage(const ImageInfo *image,
                                 ConstUnicode pathName,
                                 ImageType imageType);

EXTERN Bool ImageUtil_ReadPNGBuffer(ImageInfo *image,
                                    const unsigned char *imageData,
                                    size_t dataLen,
                                    int pngReadFlags);
EXTERN Bool ImageUtil_ConstructPNGBuffer(const ImageInfo *image, DynBuf *imageData);
EXTERN Bool ImageUtil_ConstructPNGBufferEx(const ImageInfo *image,
                                           const ImagePngWriteOptions *options,
                                           DynBuf *imageData);
EXTERN Bool ImageUtil_ConstructBMPBuffer(const ImageInfo *image, DynBuf *imageData);
EXTERN Bool ImageUtil_ConstructBuffer(const ImageInfo *image, ImageType imageType,
                                      DynBuf *imageData);
EXTERN void ImageUtil_FreeImageData(ImageInfo *image);

EXTERN Bool ImageUtil_CopyImageRect(const ImageInfo *srcImage,
                                    uint32 x,
                                    uint32 y,
                                    uint32 width,
                                    uint32 height,
                                    ImageInfo *dstImage);

#endif // _IMAGEUTIL_H_
