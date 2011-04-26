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
 *      Image manipulation routines.
 */

#ifndef _IMAGEUTIL_H_
#define _IMAGEUTIL_H_

#include "vmware.h"
#include "imageUtilTypes.h"
#include "dynbuf.h"
#include "unicodeTypes.h"

#ifdef _WIN32
#include "imageUtilWin32.h"
#endif

EXTERN Bool ImageUtil_ReadPNG(ImageInfo *image,
                              ConstUnicode pathName,
                              int pngReadFlags);
EXTERN Bool ImageUtil_WritePNG(const ImageInfo *image,
                               ConstUnicode pathName,
                               const ImagePngWriteOptions *options);

EXTERN Bool ImageUtil_WriteImage(const ImageInfo *image,
                                 ConstUnicode pathName,
                                 ImageType imageType);

EXTERN Bool ImageUtil_ReadPNGBuffer(ImageInfo *image,
                                    const unsigned char *imageData,
                                    size_t dataLen,
                                    int pngReadFlags);
EXTERN Bool ImageUtil_ConstructPNGBuffer(const ImageInfo *image,
                                         const ImagePngWriteOptions *options,
                                         DynBuf *imageData);
EXTERN Bool ImageUtil_ConstructBMPBuffer(const ImageInfo *image, DynBuf *imageData);

EXTERN void ImageUtil_FreeImageData(ImageInfo *image);

EXTERN Bool ImageUtil_Combine(const ImageInfo *images, const VMPoint *origins,
                              size_t n, ImageInfo *combined);

EXTERN Bool ImageUtil_CropAndScaleImage(ImageInfo *image,
                                        VMRect srcRect,
                                        VMRect dstRect,
                                        ImageInfo *dstImage);

#endif // _IMAGEUTIL_H_
