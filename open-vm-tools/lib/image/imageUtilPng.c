/*********************************************************
 * Copyright (C) 2002 VMware, Inc. All rights reserved.
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
 * imageUtilPng.c -- 
 *
 *      This file contains platform independent code to read and write PNG files
 *      using libpng. 
 */

#include "vmware.h"
#include "imageUtil.h"
#include "rasterConv.h"
#include "util.h"

#include <png.h>
#include <stdlib.h>

#define PNG_HEADER_CHECK_BUF_SIZE  8

typedef struct ImageUtilReadPng {
   unsigned char *data;
   unsigned int offset;
} ImageUtilReadPng;

/*
 *-----------------------------------------------------------------------------
 *
 * ImageUtilReadPngCallback --
 *
 *      Callback function for reading from a PNG file.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Fills buffer `data' by reading from the file.  Note that png_error()
 *      calls longjmp() so that an error will unwind the stack.
 *
 *-----------------------------------------------------------------------------
 */

void
ImageUtilReadPngCallback(png_structp png_ptr,   // IN: PNG file info structure
                         png_bytep data,        // OUT: input buffer
                         png_size_t length)     // IN: byte count
{
   ImageUtilReadPng *pngData = png_get_io_ptr(png_ptr);
   memcpy(data, pngData->data + pngData->offset, length);
   pngData->offset += (unsigned int)length;
}


/*
 *----------------------------------------------------------------------------
 *
 * ImageUtil_ReadPNGBuffer --
 *
 *      Loads and reads the specified PNG file and returns its attributes and
 *      data in the provided out parameters. 
 *
 * Results:
 *      Lots. 
 *
 * Side effects:
 *      Reads PNG from disk. 
 *
 *----------------------------------------------------------------------------
 */

Bool 
ImageUtil_ReadPNGBuffer(ImageInfo *image,          // OUT
                        const unsigned char *data, // IN
                        size_t dataLen)            // IN
{
   png_structp png_ptr;
   png_infop info_ptr;
   int i, channel_depth, color_type, interlace_type, compression_type, filter_type;
   png_colorp palette;
   int num_palette = 0; 
   int bytes_per_line;
   png_bytep *row_pointers = NULL;
   Bool ret = FALSE;
   ImageUtilReadPng *pngData = NULL;
   png_uint_32 width;
   png_uint_32 height;

   if (!image || !data || !dataLen) {
      return FALSE;
   }

   pngData = Util_SafeCalloc(1, sizeof *pngData);
   pngData->data = (char *) data;
   pngData->offset = 0;

   /* 
    * Do an initial check to make sure this is a PNG file. This check also
    * eliminate the case of 0-byte file due to the previous write error
    */
   if (dataLen < PNG_HEADER_CHECK_BUF_SIZE) {
      goto exit;
   }

   if (png_sig_cmp(pngData->data, 0, PNG_HEADER_CHECK_BUF_SIZE)) {
      goto exit;
   }

   png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

   if (png_ptr == NULL) {
      goto exit;
   }

   /* Allocate/initialize the memory for image information. */
   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL) {
      png_destroy_read_struct(&png_ptr, NULL, NULL);
      goto exit;
   }

   if (setjmp(png_ptr->jmpbuf)) {
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      goto exit;
   }

   png_set_read_fn(png_ptr, pngData, ImageUtilReadPngCallback);

   png_read_info(png_ptr, info_ptr);

   png_get_IHDR(png_ptr, info_ptr, 
                &width, &height,
                &channel_depth, &color_type,
                &interlace_type, &compression_type, &filter_type);

   bytes_per_line = png_get_rowbytes(png_ptr, info_ptr);

   if (color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
      /*
       * Strip out alpha
       */
      png_set_strip_alpha(png_ptr);

      /* Update the bytes_per_line now that we've eliminated the alpha channel */
      png_read_update_info(png_ptr, info_ptr);
      bytes_per_line = png_get_rowbytes(png_ptr, info_ptr);

      png_get_IHDR(png_ptr, info_ptr, 
                   &width, &height, 
                   &channel_depth, &color_type, &interlace_type, 
                   &compression_type, &filter_type);
      image->bpp = 24;
   } else if (color_type == PNG_COLOR_TYPE_RGB) {
      image->bpp = 24;  
   } else if (color_type == PNG_COLOR_TYPE_PALETTE) {
      /*
       * Load palette
       */
      if (channel_depth < 8) {
         png_set_packing(png_ptr);
         png_read_update_info(png_ptr, info_ptr);
         bytes_per_line = png_get_rowbytes(png_ptr, info_ptr);
      }

      image->bpp = 8; 

      png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette);
      ASSERT(num_palette <= 256); 

      for (i = 0; i < num_palette; i++) {
         image->palette[i].red = palette[i].red;
         image->palette[i].green = palette[i].green;
         image->palette[i].blue = palette[i].blue;
         image->palette[i].reserved = 0;
      }

      image->numColors = num_palette; 
   } else {
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      goto exit;
   }

   image->width = width;
   image->height = height;
   image->bytesPerLine = DWORD_ALIGN(bytes_per_line);
   image->depth = image->bpp;
   image->flags = 0;

   /* BGR instead of RGB - Intel byte-order is backwards */
   png_set_bgr(png_ptr);

   /* Allocate the memory to hold the image using the fields of info_ptr. */
   image->data = Util_SafeMalloc(image->bytesPerLine * image->height);
   row_pointers = Util_SafeMalloc(sizeof *row_pointers * image->height);

   /* The easiest way to read the image: */
   for (i = 0; i < image->height; i++) {
      row_pointers[i] = image->data + i * (DWORD_ALIGN(bytes_per_line));
   }

   png_read_image(png_ptr, row_pointers);
   png_read_end(png_ptr, info_ptr);

#ifdef VMX86_DEVEL
   /* Read text fields (XXX eventually read rectangle data here) */
   {
      png_text *text_ptr = NULL;
      int num_text = 0;

      if (png_get_text(png_ptr, info_ptr, &text_ptr, &num_text) > 0) {
         for(i = 0; i < num_text; i++) {
            fprintf(stderr, "Png text: (%s) %s\n", text_ptr[i].key,
                    text_ptr[i].text);
         }
      }
   }
#endif

   png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
   ret = TRUE;

  exit:
   free(row_pointers);
   free(pngData);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * ImageUtilDataWriteCallback
 *
 *      Callback for the png library to write data into the DynBuf.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Writes data to DynBuf.
 *
 *----------------------------------------------------------------------------
 */

void
ImageUtilDataWriteCallback(png_structp png_ptr,         // IN
                           png_bytep data,              // IN
                           png_size_t length)           // IN
{
   DynBuf *imageData = png_get_io_ptr(png_ptr);
   if (!DynBuf_Append(imageData, data, length)) {
      png_error(png_ptr, "Unable to append data");
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * ImageUtil_ConstructPNGBuffer --
 *
 *      Writes a PNG of the image to the DynBuf passed in.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *      If successful, imageData should be destroyed.
 *
 *----------------------------------------------------------------------------
 */

Bool
ImageUtil_ConstructPNGBuffer(const ImageInfo *image, // IN
                             DynBuf *imageData)      // OUT

{
   ImagePngOptions options;

   options.zlibCompressLevel = -1;
   options.stripAlphaChannel = TRUE;

   return ImageUtil_ConstructPNGBufferEx(image, &options, imageData);
}


/*
 *----------------------------------------------------------------------------
 *
 * ImageUtil_ConstructPNGBufferEx --
 *
 *      Writes a PNG of the image to the DynBuf passed in. Accepts a zlib
 *      compression level (0-9, 0 means no compression, -1 means "use the
 *      default").
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *      Allocates memory; If successful, imageData needs to be cleaned up later.
 *
 *----------------------------------------------------------------------------
 */

Bool
ImageUtil_ConstructPNGBufferEx(const ImageInfo *image,         // IN
                               const ImagePngOptions *options, // IN
                               DynBuf *imageData)              // OUT
{
   png_structp png_ptr;
   png_infop info_ptr;
   int color_type, i;
   png_bytep data;
   
   //png_text text_ptr[1];
#ifdef RGB_MASK
   png_color_8 sig_bit;
#endif

   png_uint_32 k;

   int bytes_per_line;
   png_bytep *row_pointers;

   ASSERT(image);
   ASSERT(imageData);
   ASSERT(options);

   if (!image || !options || !imageData) {
      return FALSE;
   }

   row_pointers = malloc(sizeof *row_pointers * image->height);
   if (!row_pointers) {
      return FALSE;
   }

   DynBuf_Init(imageData);

   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

   if (png_ptr == NULL) {
      goto error;
   }

   /* Allocate/initialize the image information data.  REQUIRED */
   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL) {
      png_destroy_write_struct(&png_ptr, NULL);
      goto error;
   }

   if (setjmp(png_ptr->jmpbuf)) {
      png_destroy_write_struct(&png_ptr, &info_ptr);
      goto error;
   }

   png_set_write_fn(png_ptr, imageData, ImageUtilDataWriteCallback, NULL);

   if (image->bpp <= 8) {
      /*
       * Save palette
       */
      color_type = PNG_COLOR_TYPE_PALETTE;
   } else if (image->bpp == 32) {
      /*
       * Save with alpha channel
       */
      color_type = PNG_COLOR_TYPE_RGB_ALPHA;
   } else {
      /*
       * Everything else is RGB
       */
      color_type = PNG_COLOR_TYPE_RGB;
   }

   png_set_IHDR(png_ptr, info_ptr, image->width, image->height, 8, color_type,
                PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

   if (options->zlibCompressLevel >= 0 && options->zlibCompressLevel <= 9) {
      png_set_compression_level(png_ptr, options->zlibCompressLevel);
   }

#ifdef RGB_MASK
   /* Setup the color depths */
   sig_bit.red = COUNT_BITS(image->redMask);
   sig_bit.green = COUNT_BITS(image->greenMask);
   sig_bit.blue = COUNT_BITS(image->blueMask);
   png_set_sBIT(png_ptr, info_ptr, &sig_bit);

#if 1 //We probably DO need this
   /* Shift the pixels up to a legal bit depth and fill in
    * as appropriate to correctly scale the image.
    */
   png_set_shift(png_ptr, &sig_bit);
#endif
#endif

   // BGR instead of RGB - Intel byte-order is backwards
   png_set_bgr(png_ptr);

   /* 
    * Optionally write comments into the image, e.g.
    *   text_ptr[0].key = "Title";
    *   text_ptr[0].text = "Mona Lisa";
    *   text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
    *   png_set_text(png_ptr, info_ptr, text_ptr, 1);
    */


   /* 
    * Now set up the transformations you want.  Note that these are
    * all optional.  Only call them if you want them.
    */
   bytes_per_line = image->bytesPerLine;

#if 0
   /* pack pixels into bytes */
   png_set_packing(png_ptr);
#endif

   data = image->data;

   if (image->bpp == 24) {
      /*
       * The image is already RGB, no need for any conversion/processing
       */
   } else if (image->bpp <= 8) {
      /*
       * Save palette
       */
      png_color palette[256];
      ASSERT(image->numColors <= 256);

      for (i = 0; i < image->numColors; i++) {
         palette[i].red = image->palette[i].red;
         palette[i].green = image->palette[i].green;
         palette[i].blue = image->palette[i].blue;
      }

      png_set_PLTE(png_ptr, info_ptr, palette, image->numColors);
   } else if (image->bpp == 32) {
      if (options->stripAlphaChannel) {

         /*
          * Strip the alpha channel
          */

         png_set_strip_alpha(png_ptr);

         /*
          * XXX: Following call appears to leak two allocations of 1 byte
          * each deep inside the png library via png_malloc.
          */
         png_read_update_info(png_ptr, info_ptr);
      }
   } else {
      /* XXX convert to 24 - we could try and support other modes? */
      bytes_per_line = DWORD_ALIGN(png_get_rowbytes(png_ptr, info_ptr));

      data = Util_SafeMalloc(bytes_per_line * image->height);

      Raster_ConvertPixels(data, bytes_per_line, 24,
                           image->data, image->bytesPerLine, image->depth,
                           FALSE, NULL, 0, 0, 0, 0,
                           image->width, image->height);
   }

   /* Write the file header information.  REQUIRED */
   png_write_info(png_ptr, info_ptr);

   if ((image->bpp == 32) && options->stripAlphaChannel) {
      /* treat the alpha channel byte as filler */
      png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
   }

   for (k = 0; k < image->height; k++) {
      int rowIndex;

      if (image->flags & IMAGE_FLAG_BOTTOM_UP) {
         rowIndex = image->height - 1 - k;
      } else {
         rowIndex = k;
      }

      row_pointers[rowIndex] = data + k * bytes_per_line;
   }

   png_write_image(png_ptr, row_pointers);

   if (data != image->data) {
      free(data);
   }

   /* It is REQUIRED to call this to finish writing the rest of the file */
   png_write_end(png_ptr, info_ptr);

   /* if you allocated any text comments, free them here */

   png_destroy_write_struct(&png_ptr, &info_ptr);

   free(row_pointers);

   return TRUE;
  error:
   free(row_pointers);
   DynBuf_Destroy(imageData);
   return FALSE;
}

