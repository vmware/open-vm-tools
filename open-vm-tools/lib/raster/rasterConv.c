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
 * rasterConv.c --
 *
 *	Pixel conversion routines
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "vmware.h"
#include "rasterConv.h"

#define CONVERT_LONG_TO_SHORT(pix, redMask, greenMask, blueMask,	\
			      redShift, greenShift, blueShift)		\
   (((redMask) & ((pix) >> (redShift))) |				\
    ((greenMask) & ((pix) >> (greenShift))) |				\
    ((blueMask) & ((pix) >> (blueShift))))

#define CONVERT_SHORT_TO_LONG(pix, redMask, greenMask, blueMask,	\
			      redShift1, redShift2,			\
			      greenShift1, greenShift2,			\
			      blueShift1, blueShift2)			\
   ((REDMASK_32 &							\
     (((((pix) & (redMask)) >> (redShift1)) |				\
      (((pix) & (redMask)) >> (redShift2))) << 16)) |			\
    (GREENMASK_32 &							\
     (((((pix) & (greenMask)) >> (greenShift1)) |			\
      (((pix) & (greenMask)) >> (greenShift2))) << 8)) |		\
    (BLUEMASK_32 &							\
     ((((pix) & (blueMask)) << (blueShift1)) |				\
      (((pix) & (blueMask)) >> (blueShift2)))))

#define CONVERT_LONG_TO_8BGR(pix, redMask, greenMask, blueMask,	\
			      redShift, greenShift, blueShift)		\
   (((redMask) & ((pix) >> (redShift))) |				\
    ((greenMask) & ((pix) >> (greenShift))) |				\
    ((blueMask) & ((pix) << (blueShift))))


static const uint8 byte_masks_8bgr[9][3] = {
   /*0*/ {0,0,0},
   /*1*/ {0,0,0},
   /*2*/ {0,0,0},
   /*3*/ {REDMASK_BGR111, GREENMASK_BGR111, BLUEMASK_BGR111},
   /*4*/ {0,0,0},
   /*5*/ {0,0,0},
   /*6*/ {REDMASK_RGB222, GREENMASK_RGB222, BLUEMASK_RGB222},
   /*7*/ {0,0,0},
   /*8*/ {REDMASK_BGR233, GREENMASK_BGR233, BLUEMASK_BGR233}
};

#define REDMASK_8BPP(depth) (byte_masks_8bgr[depth][0])
#define GREENMASK_8BPP(depth) (byte_masks_8bgr[depth][1])
#define BLUEMASK_8BPP(depth) (byte_masks_8bgr[depth][2])


/*
 * Local functions
 */

static void RasterConvert15to16(uint8 *tof, uint32 line_increment,
				const uint8 *src, uint32 src_increment,
				uint32 src_x, uint32 src_y,
				uint32 x, uint32 y, uint32 w, uint32 h);
static void RasterConvertShortTo24(uint8 *tof, uint32 line_increment,
				   const uint8 *src, uint32 src_increment,
				   uint32 src_x, uint32 src_y,
				   uint32 x, uint32 y, uint32 w, uint32 h,
				   uint32 redMask, uint32 greenMask,
				   uint32 blueMask,
				   uint32 redShift1, uint32 redShift2,
				   uint32 greenShift1, uint32 greenShift2,
				   uint32 blueShift1, uint32 blueShift2);
static void RasterConvertShortTo32(uint8 *tof, uint32 line_increment,
				   const uint8 *src, uint32 src_increment,
				   uint32 src_x, uint32 src_y,
				   uint32 x, uint32 y, uint32 w, uint32 h,
				   uint32 redMask, uint32 greenMask,
				   uint32 blueMask,
				   uint32 redShift1, uint32 redShift2,
				   uint32 greenShift1, uint32 greenShift2,
				   uint32 blueShift1, uint32 blueShift2);
static void RasterConvert16to15(uint8 *tof, uint32 line_increment,
				const uint8 *src, uint32 src_increment,
				uint32 src_x, uint32 src_y,
				uint32 x, uint32 y, uint32 w, uint32 h);
static void RasterConvert24toShort(uint8 *tof, uint32 line_increment,
				   const uint8 *src, uint32 src_increment,
				   uint32 src_x, uint32 src_y,
				   uint32 x, uint32 y, uint32 w, uint32 h,
				   uint32 redMask, uint32 greenMask,
				   uint32 blueMask,
				   uint32 redShift, uint32 greenShift,
				   uint32 blueShift);
static void RasterConvert24to32(uint8 *tof, uint32 line_increment,
				const uint8 *src, uint32 src_increment,
				uint32 src_x, uint32 src_y,
				uint32 x, uint32 y, uint32 w, uint32 h);
static void RasterConvert32toShort(uint8 *tof, uint32 line_increment,
				   const uint8 *src, uint32 src_increment,
				   uint32 src_x, uint32 src_y,
				   uint32 x, uint32 y, uint32 w, uint32 h,
				   uint32 redMask, uint32 greenMask,
				   uint32 blueMask,
				   uint32 redShift, uint32 greenShift,
				   uint32 blueShift);
static void RasterConvert32to24(uint8 *tof, uint32 line_increment,
				const uint8 *src, uint32 src_increment,
				uint32 src_x, uint32 src_y,
				uint32 x, uint32 y, uint32 w, uint32 h);
static void RasterConvertIndextoShort(uint8 *tof, uint32 line_increment,
				      const uint8 *src, uint32 src_increment,
				      const uint32 *pixels,
				      uint32 src_x, uint32 src_y,
				      uint32 x, uint32 y, uint32 w, uint32 h,
				      uint32 redMask, uint32 greenMask,
				      uint32 blueMask,
				      uint32 redShift, uint32 greenShift,
				      uint32 blueShift);
static void RasterConvertIndexto24(uint8 *tof, uint32 line_increment,
				   const uint8 *src, uint32 src_increment,
				   const uint32 *pixels,
				   uint32 src_x, uint32 src_y,
				   uint32 x, uint32 y, uint32 w, uint32 h);
static void RasterConvertIndexto32(uint8 *tof, uint32 line_increment,
				   const uint8 *src, uint32 src_increment,
				   const uint32 *pixels,
				   uint32 src_x, uint32 src_y,
				   uint32 x, uint32 y, uint32 w, uint32 h);

static void RasterConvert32to8(uint8 *tof, uint32 line_increment,
			       const uint8 *src, uint32 src_increment,
			       uint32 src_x, uint32 src_y,
			       uint32 x, uint32 y, uint32 w, uint32 h,
			       uint32 redMask, uint32 greenMask, uint32 blueMask,
			       uint32 redShift, uint32 greenShift, uint32 blueShift);
static void RasterConvert24to8(uint8 *tof, uint32 line_increment,
			       const uint8 *src, uint32 src_increment,
			       uint32 src_x, uint32 src_y,
			       uint32 x, uint32 y, uint32 w, uint32 h,
			       uint32 redMask, uint32 greenMask, uint32 blueMask,
			       uint32 redShift, uint32 greenShift, uint32 blueShift);
static void RasterConvert16to8(uint8 *tof, uint32 line_increment,
                               const uint8 *src, uint32 src_increment,
                               uint32 src_x, uint32 src_y,
                               uint32 x, uint32 y, uint32 w, uint32 h,
                               uint32 redMask, uint32 greenMask, uint32 blueMask,
                               int redShift, int greenShift, int blueShift);
static void RasterConvertIndexto8(uint8 *tof, uint32 line_increment,
				  const uint8 *src, uint32 src_increment,
                                  const uint32 *pixels,
				  uint32 src_x, uint32 src_y,
				  uint32 x, uint32 y, uint32 w, uint32 h,
				  uint32 redMask, uint32 greenMask, uint32 blueMask,
				  uint32 redShift, uint32 greenShift, uint32 blueShift);
static int RasterGetShiftFromMask(uint32 start, uint32 mask);
static INLINE uint32 RasterShiftPixel(uint32 pixel, int shift);


/*
 *----------------------------------------------------------------------
 *
 * Raster_IsModeReasonable --
 *
 *	Determine if a mode is something that Raster_Convert*
 *      can deal with.
 *
 * Results:
 *      TRUE if we like the mode.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
Raster_IsModeReasonable(uint32 depth, uint32 bpp, Bool pseudocolor)
{
   return (pseudocolor && bpp == 8) || (!pseudocolor &&
                                        ((bpp == 16 && (depth == 15 ||
                                                        depth == 16)) ||
                                         (bpp == 24 && depth == 24) ||
                                         (bpp == 32 && depth == 24)));
}


/*
 *----------------------------------------------------------------------
 *
 * Raster_GetBPPDepth --
 *
 *      Converts separate depth and bpp values into one "bppdepth."
 *      See comment above Raster_ConvertPixels().
 *
 * Results:
 *      The bppdepth.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Raster_GetBPPDepth(uint32 depth, uint32 bpp)
{
   if (depth == 24 && bpp == 32) {
      return 32;
   } else {
      return depth;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * Raster_ConvertPixels --
 *
 *	Convert pixels from one depth to another, while copying from
 *	source to destination.
 *
 *      bppdepth is a unique number specifying the bpp/color-depth,
 *      like so:
 *
 *      bpp   depth    bppdepth
 *      -----------------------
 *       8      3         3
 *       8      6         6
 *       8      8         8
 *      16     15        15
 *      16     16        16
 *      24     24        24
 *      32     24        32 * This is the only one that differs from depth
 *
 * Results:
 *	None
 *
 * Side effects:
 *      If converting from a lower true-color depth to 32-bpp, fills the
 *      alpha values of the destination rectangle to 0xFF.
 *
 *----------------------------------------------------------------------
 */

void
Raster_ConvertPixels(uint8 *tof, int32 line_increment, int bppdepth,
		     const uint8 *src, int32 src_increment, int src_bppdepth,
		     Bool pseudoColor, const uint32 *pixels,
		     uint32 src_x, uint32 src_y,
		     uint32 x, uint32 y, uint32 w, uint32 h)
{
   int redShift, greenShift, blueShift;

   if (pseudoColor) {
      if (src_bppdepth > 8) {
	 Warning("Raster convert pixels invalid depth for pseudo color %d\n",
		 src_bppdepth);
	 NOT_IMPLEMENTED();
	 return;
      }

      switch (bppdepth) {
	 case 3: case 6: case 8:
            redShift = RasterGetShiftFromMask(24, REDMASK_8BPP(bppdepth));
            greenShift = RasterGetShiftFromMask(16, GREENMASK_8BPP(bppdepth));
            blueShift = RasterGetShiftFromMask(8, BLUEMASK_8BPP(bppdepth));
            ASSERT(redShift >= 0 && greenShift >= 0 && blueShift >= 0);
	    
            RasterConvertIndexto8(tof, line_increment, src, src_increment,
				  pixels, src_x, src_y, x, y, w, h,
				  REDMASK_8BPP(bppdepth),
				  GREENMASK_8BPP(bppdepth), 
				  BLUEMASK_8BPP(bppdepth),
                                  redShift, greenShift, blueShift);
	    break;

	 case 15:
	    RasterConvertIndextoShort(tof, line_increment, src, src_increment,
				      pixels, src_x, src_y, x, y, w, h,
				      REDMASK_15, GREENMASK_15, BLUEMASK_15,
				      9, 6, 3);
	    break;

	 case 16:
	    RasterConvertIndextoShort(tof, line_increment, src, src_increment,
				      pixels, src_x, src_y, x, y, w, h,
				      REDMASK_16, GREENMASK_16, BLUEMASK_16,
				      8, 5, 3);
	    break;

	 case 24:
	    RasterConvertIndexto24(tof, line_increment, src, src_increment,
				   pixels, src_x, src_y, x, y, w, h);
	    break;

	 case 32:
	    RasterConvertIndexto32(tof, line_increment, src, src_increment,
				   pixels, src_x, src_y, x, y, w, h);
	    break;

	 default:
	    Warning("Raster convert pixels invalid depth %d\n", bppdepth);
	    NOT_IMPLEMENTED();
	    break;
      }
      return;
   }

   switch (src_bppdepth) {
      case 15:
	 switch (bppdepth) {
	    case 3: case 6: case 8:
               redShift = RasterGetShiftFromMask(15, REDMASK_8BPP(bppdepth));
               greenShift = RasterGetShiftFromMask(10, GREENMASK_8BPP(bppdepth));
               blueShift = RasterGetShiftFromMask(5, BLUEMASK_8BPP(bppdepth));

	       RasterConvert16to8(tof, line_increment, src, src_increment,
                                  src_x, src_y, x, y, w, h,
                                  REDMASK_8BPP(bppdepth),
                                  GREENMASK_8BPP(bppdepth), 
                                  BLUEMASK_8BPP(bppdepth),
                                  redShift, greenShift, blueShift);
	       break; 

	    case 15:
	       Warning("Raster convert called when no conversion needed\n");
	       NOT_IMPLEMENTED();
	       break;

	    case 16:
	       RasterConvert15to16(tof, line_increment, src, src_increment,
				   src_x, src_y, x, y, w, h);
	       break;

	    case 24:
	       RasterConvertShortTo24(tof, line_increment, src, src_increment,
				      src_x, src_y, x, y, w, h,
				      REDMASK_15, GREENMASK_15, BLUEMASK_15,
				      7, 12, 2, 7, 3, 2);
	       break;

	    case 32:
	       RasterConvertShortTo32(tof, line_increment, src, src_increment,
				      src_x, src_y, x, y, w, h,
				      REDMASK_15, GREENMASK_15, BLUEMASK_15,
				      7, 12, 2, 7, 3, 2);
	       break;

	    default:
	       Warning("Raster convert pixels invalid depth %d\n", bppdepth);
	       NOT_IMPLEMENTED();
	       break;
	 }
	 break;

      case 16:
	 switch (bppdepth) {
	    case 3: case 6: case 8:
               redShift = RasterGetShiftFromMask(16, REDMASK_8BPP(bppdepth));
               greenShift = RasterGetShiftFromMask(11, GREENMASK_8BPP(bppdepth));
               blueShift = RasterGetShiftFromMask(5, BLUEMASK_8BPP(bppdepth));

 	       RasterConvert16to8(tof, line_increment, src, src_increment,
                                  src_x, src_y, x, y, w, h,
                                  REDMASK_8BPP(bppdepth),
                                  GREENMASK_8BPP(bppdepth), 
                                  BLUEMASK_8BPP(bppdepth),
                                  redShift, greenShift, blueShift);
	       break;

	    case 15:
	       RasterConvert16to15(tof, line_increment, src, src_increment,
				   src_x, src_y, x, y, w, h);
	       break;

	    case 16:
	       Warning("Raster convert called when no conversion needed\n");
	       NOT_IMPLEMENTED();
	       break;

	    case 24:
	       RasterConvertShortTo24(tof, line_increment, src, src_increment,
				      src_x, src_y, x, y, w, h,
				      REDMASK_16, GREENMASK_16, BLUEMASK_16,
				      8, 13, 3, 9, 3, 2);
	       break;

	    case 32:
	       RasterConvertShortTo32(tof, line_increment, src, src_increment,
				      src_x, src_y, x, y, w, h,
				      REDMASK_16, GREENMASK_16, BLUEMASK_16,
				      8, 13, 3, 9, 3, 2);
	       break;

	    default:
	       Warning("Raster convert pixels invalid depth %d\n", bppdepth);
	       NOT_IMPLEMENTED();
	       break;
	 }
	 break;

      case 24:
	 switch (bppdepth) {
	    case 3: case 6: case 8:
               redShift = RasterGetShiftFromMask(8, REDMASK_8BPP(bppdepth));
               greenShift = RasterGetShiftFromMask(8, GREENMASK_8BPP(bppdepth));
               blueShift = RasterGetShiftFromMask(8, BLUEMASK_8BPP(bppdepth));
               ASSERT(redShift >= 0 && greenShift >= 0 && blueShift >= 0);

	       RasterConvert24to8(tof, line_increment, src, src_increment,
				  src_x, src_y, x, y, w, h,
				  REDMASK_8BPP(bppdepth),
				  GREENMASK_8BPP(bppdepth), 
				  BLUEMASK_8BPP(bppdepth),
				  redShift, greenShift, blueShift);
	       break;

	    case 15:
	       RasterConvert24toShort(tof, line_increment, src, src_increment,
				      src_x, src_y, x, y, w, h,
				      REDMASK_15, GREENMASK_15, BLUEMASK_15,
				      7, 2, 3);
	       break;

	    case 16:
	       RasterConvert24toShort(tof, line_increment, src, src_increment,
				      src_x, src_y, x, y, w, h,
				      REDMASK_16, GREENMASK_16, BLUEMASK_16,
				      8, 3, 3);
	       break;

	    case 24:
	       Warning("Raster convert called when no conversion needed\n");
	       NOT_IMPLEMENTED();
	       break;

	    case 32:
	       RasterConvert24to32(tof, line_increment, src, src_increment,
				   src_x, src_y, x, y, w, h);
	       break;

	    default:
	       Warning("Raster convert pixels invalid depth %d\n", bppdepth);
	       NOT_IMPLEMENTED();
	       break;
	 }
	 break;

      case 32:
	 switch (bppdepth) {
	    case 3: case 6: case 8:
               redShift = RasterGetShiftFromMask(24, REDMASK_8BPP(bppdepth));
               greenShift = RasterGetShiftFromMask(16, GREENMASK_8BPP(bppdepth));
               blueShift = RasterGetShiftFromMask(8, BLUEMASK_8BPP(bppdepth));
               ASSERT(redShift >= 0 && greenShift >= 0 && blueShift >= 0);

	       RasterConvert32to8(tof, line_increment, src, src_increment,
				  src_x, src_y, x, y, w, h,
				  REDMASK_8BPP(bppdepth),
				  GREENMASK_8BPP(bppdepth), 
				  BLUEMASK_8BPP(bppdepth),
				  redShift, greenShift, blueShift);
	       break;

	    case 15:
	       RasterConvert32toShort(tof, line_increment, src, src_increment,
				      src_x, src_y, x, y, w, h,
				      REDMASK_15, GREENMASK_15, BLUEMASK_15,
				      9, 6, 3);
	       break;

	    case 16:
	       RasterConvert32toShort(tof, line_increment, src, src_increment,
				      src_x, src_y, x, y, w, h,
				      REDMASK_16, GREENMASK_16, BLUEMASK_16,
				      8, 5, 3);
	       break;

	    case 24:
	       RasterConvert32to24(tof, line_increment, src, src_increment,
				   src_x, src_y, x, y, w, h);
	       break;

	    case 32:
	       Warning("Raster convert called when no conversion needed\n");
	       NOT_IMPLEMENTED();
	       break;

	    default:
	       Warning("Raster convert pixels invalid depth %d\n", bppdepth);
	       NOT_IMPLEMENTED();
	       break;
	 }
	 break;

      default:
	 Warning("Raster convert pixels invalid source depth %d\n", src_bppdepth);
	 NOT_IMPLEMENTED_BUG(10982);
	 break;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * Raster_ConvertOnePixel --
 *
 *	Convert the given pixel from its current depth to the specified
 *	depth.
 *
 * Results:
 *	The converted pixel
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

uint32
Raster_ConvertOnePixel(uint32 pix, int src_bppdepth, int bppdepth,
		       Bool pseudoColor, const uint32 *pixels)
{
   if (pseudoColor) {
      if (src_bppdepth != 8) {
	 Warning("Raster convert pixels invalid depth for pseudo color %d\n",
		 src_bppdepth);
	 NOT_IMPLEMENTED();
	 return 0;
      }
      pix = pixels[pix];
      src_bppdepth = 32;
   }

   switch (src_bppdepth) {
      case 15:
	 switch (bppdepth) {
	    case 3: case 6: case 8:
	       return CONVERT_LONG_TO_8BGR(pix, REDMASK_8BPP(bppdepth), 
					   GREENMASK_8BPP(bppdepth),
					   BLUEMASK_8BPP(bppdepth), 12, 4, 3);

	    case 15:
	       return pix;

	    case 16:
	       return ((pix & (REDMASK_15 | GREENMASK_15)) << 1) |
		      ((pix & GREEN_HIBIT_15) >> GREEN_HILOSHIFT_15) |
		      (pix & BLUEMASK_16);

	    case 24:
	    case 32:
	       return CONVERT_SHORT_TO_LONG(pix, REDMASK_15, GREENMASK_15,
					    BLUEMASK_15, 7, 12, 2, 7, 3, 2);

	    default:
	       Warning("Raster convert one pixel invalid depth %d\n", bppdepth);
	       NOT_IMPLEMENTED();
	       return 0;
	 }

      case 16:
	 switch (bppdepth) {
	    case 3: case 6: case 8:
	       return CONVERT_LONG_TO_8BGR(pix, REDMASK_8BPP(bppdepth), 
					   GREENMASK_8BPP(bppdepth),
					   BLUEMASK_8BPP(bppdepth), 13, 5, 3);

	    case 15:
	       return ((pix >> 1) & (REDMASK_15 | GREENMASK_15)) |
		      (pix & BLUEMASK_15);

	    case 16:
	       return pix;

	    case 24:
	    case 32:
	       return CONVERT_SHORT_TO_LONG(pix, REDMASK_16, GREENMASK_16,
					    BLUEMASK_16, 8, 13, 3, 9, 3, 2);

	    default:
	       Warning("Raster convert one pixel invalid depth %d\n", bppdepth);
	       NOT_IMPLEMENTED();
	       return 0;
	 }

      case 24:
      case 32:
	 switch (bppdepth) {
	    case 3: case 6: case 8:
	       return CONVERT_LONG_TO_8BGR(pix, REDMASK_8BPP(bppdepth), 
					   GREENMASK_8BPP(bppdepth),
					   BLUEMASK_8BPP(bppdepth), 21, 10, 0);

	    case 15:
	       return CONVERT_LONG_TO_SHORT(pix, REDMASK_15, GREENMASK_15,
					    BLUEMASK_15, 9, 6, 3);

	    case 16:
	       return CONVERT_LONG_TO_SHORT(pix, REDMASK_16, GREENMASK_16,
					    BLUEMASK_16, 8, 5, 3);

	    case 24:
	    case 32:
	       return pix;

	    default:
	       Warning("Raster convert one pixel invalid depth %d\n", bppdepth);
	       NOT_IMPLEMENTED();
	       return 0;
	 }

      default:
	 Warning("Raster convert one pixel invalid source depth %d\n",
		 src_bppdepth);
	 NOT_IMPLEMENTED();
	 return 0;
   }

   return pix;
}


/*
 *----------------------------------------------------------------------
 *
 * Raster_ConversionParameters --
 *
 *	Get component masks that the conversion routines use for the
 *	supported depths
 *
 * Results:
 *	Returns FALSE if depth not supported
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

Bool
Raster_ConversionParameters(int bppdepth,      // IN
                            uint32 *redMask,   // OUT
			    uint32 *greenMask, // OUT
                            uint32 *blueMask)  // OUT
{
   switch (bppdepth) {
      case 15:
	 *redMask = REDMASK_15;
	 *greenMask = GREENMASK_15;
	 *blueMask = BLUEMASK_15;
	 break;

      case 16:
	 *redMask = REDMASK_16;
	 *greenMask = GREENMASK_16;
	 *blueMask = BLUEMASK_16;
	 break;

      case 24:
	 *redMask = REDMASK_24;
	 *greenMask = GREENMASK_24;
	 *blueMask = BLUEMASK_24;
	 break;

      case 32:
	 *redMask = REDMASK_32;
	 *greenMask = GREENMASK_32;
	 *blueMask = BLUEMASK_32;
	 break;

      default:
	 return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * RasterConvert15to16 --
 *
 *	Convert pixels from depth 15 to depth 16, while copying from
 *	source to destination.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
RasterConvert15to16(uint8 *tof, uint32 line_increment,
		    const uint8 *src, uint32 src_increment,
		    uint32 src_x, uint32 src_y,
		    uint32 x, uint32 y, uint32 w, uint32 h)
{
   const uint16 *srcptr;
   uint16 *dstptr;
   int i, j;

   src_increment >>= 1;
   srcptr = (uint16 *)src;
   srcptr += (src_y * src_increment) + src_x;

   line_increment >>= 1;
   dstptr = (uint16 *)tof;
   dstptr += (y * line_increment) + x;

   for (i=0; i<h; i++) {
      for (j=0; j<w; j++) {
	 uint32 pix = srcptr[j];
	 dstptr[j] = ((pix & (REDMASK_15 | GREENMASK_15)) << 1) |
		     ((pix & GREEN_HIBIT_15) >> GREEN_HILOSHIFT_15) |
		     (pix & BLUEMASK_16);
      }
      srcptr += src_increment;
      dstptr += line_increment;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterConvertShortTo24 --
 *
 *	Convert pixels from depth 15 or 16 to depth 24, while copying
 *	from source to destination.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
RasterConvertShortTo24(uint8 *tof, uint32 line_increment,
		       const uint8 *src, uint32 src_increment,
		       uint32 src_x, uint32 src_y,
		       uint32 x, uint32 y, uint32 w, uint32 h,
		       uint32 redMask, uint32 greenMask, uint32 blueMask,
		       uint32 redShift1, uint32 redShift2,
		       uint32 greenShift1, uint32 greenShift2,
		       uint32 blueShift1, uint32 blueShift2)
{
   const uint16 *srcptr;
   uint8 *dstptr;
   int i, j, k;

   src_increment >>= 1;
   srcptr = (uint16 *)src;
   srcptr += (src_y * src_increment) + src_x;

   dstptr = tof;
   dstptr += (y * line_increment) + (x * 3);

   for (i=0; i<h; i++) {
      for (j=0, k=0; j<w; j++) {
	 uint32 pix = srcptr[j];
	 dstptr[k++] = ((pix & blueMask) << blueShift1) |
		       ((pix & blueMask) >> blueShift2);
	 dstptr[k++] = ((pix & greenMask) >> greenShift1) |
		       ((pix & greenMask) >> greenShift2);
	 dstptr[k++] = ((pix & redMask) >> redShift1) |
		       ((pix & redMask) >> redShift2);
      }
      srcptr += src_increment;
      dstptr += line_increment;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterConvertShortTo32 --
 *
 *	Convert pixels from depth 15 or 16 to depth 32, while copying
 *	from source to destination.
 *
 * Results:
 *	None
 *
 * Side effects:
 *      Fills the alpha values of the destination rectangle to 0xFF.
 *
 *----------------------------------------------------------------------
 */

static void
RasterConvertShortTo32(uint8 *tof, uint32 line_increment,
		       const uint8 *src, uint32 src_increment,
		       uint32 src_x, uint32 src_y,
		       uint32 x, uint32 y, uint32 w, uint32 h,
		       uint32 redMask, uint32 greenMask, uint32 blueMask,
		       uint32 redShift1, uint32 redShift2,
		       uint32 greenShift1, uint32 greenShift2,
		       uint32 blueShift1, uint32 blueShift2)
{
   const uint16 *srcptr;
   uint32 *dstptr;
   int i, j;

   src_increment >>= 1;
   srcptr = (uint16 *)src;
   srcptr += (src_y * src_increment) + src_x;

   line_increment >>= 2;
   dstptr = (uint32 *)tof;
   dstptr += (y * line_increment) + x;

   for (i=0; i<h; i++) {
      for (j=0; j<w; j++) {
	 uint32 pix = srcptr[j];
	 dstptr[j] = (0xFF << 24) |
                     (REDMASK_32 &
		      ((((pix & redMask) >> redShift1) |
			((pix & redMask) >> redShift2)) << 16)) |
		     (GREENMASK_32 &
		      ((((pix & greenMask) >> greenShift1) |
			((pix & greenMask) >> greenShift2)) << 8)) |
		     (BLUEMASK_32 &
		      (((pix & blueMask) << blueShift1) |
		       ((pix & blueMask) >> blueShift2)));
      }
      srcptr += src_increment;
      dstptr += line_increment;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterConvert16to15 --
 *
 *	Convert pixels from depth 16 to depth 15, while copying from
 *	source to destination.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
RasterConvert16to15(uint8 *tof, uint32 line_increment,
		    const uint8 *src, uint32 src_increment,
		    uint32 src_x, uint32 src_y,
		    uint32 x, uint32 y, uint32 w, uint32 h)
{
   const uint16 *srcptr;
   uint16 *dstptr;
   int i, j;

   src_increment >>= 1;
   srcptr = (uint16 *)src;
   srcptr += (src_y * src_increment) + src_x;

   line_increment >>= 1;
   dstptr = (uint16 *)tof;
   dstptr += (y * line_increment) + x;

   for (i=0; i<h; i++) {
      for (j=0; j<w; j++) {
	 uint32 pix = srcptr[j];
	 dstptr[j] = ((pix >> 1) & (REDMASK_15 | GREENMASK_15)) |
		     (pix & BLUEMASK_15);
      }
      srcptr += src_increment;
      dstptr += line_increment;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterConvert24toShort --
 *
 *	Convert pixels from depth 24 to depth 15 or 16, while copying
 *	from source to destination.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
RasterConvert24toShort(uint8 *tof, uint32 line_increment,
		       const uint8 *src, uint32 src_increment,
		       uint32 src_x, uint32 src_y,
		       uint32 x, uint32 y, uint32 w, uint32 h,
		       uint32 redMask, uint32 greenMask, uint32 blueMask,
		       uint32 redShift, uint32 greenShift, uint32 blueShift)
{
   const uint8 *srcptr;
   uint16 *dstptr;
   int i, j, k;

   srcptr = src;
   srcptr += (src_y * src_increment) + (src_x * 3);

   line_increment >>= 1;
   dstptr = (uint16 *)tof;
   dstptr += (y * line_increment) + x;

   for (i=0; i<h; i++) {
      for (j=0, k=0; j<w; j++) {
	 uint8 blue = srcptr[k++];
	 uint8 green = srcptr[k++];
	 uint8 red = srcptr[k++];
	 dstptr[j] = ((red << redShift) & redMask) |
		     ((green << greenShift) & greenMask) |
		     ((blue >> blueShift) & blueMask);
      }
      srcptr += src_increment;
      dstptr += line_increment;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterConvert24to32 --
 *
 *	Convert pixels from depth 24 to depth 32, while copying
 *	from source to destination.
 *
 * Results:
 *	None
 *
 * Side effects:
 *      Fills the alpha values of the destination rectangle to 0xFF.
 *
 *----------------------------------------------------------------------
 */

static void
RasterConvert24to32(uint8 *tof, uint32 line_increment,
		    const uint8 *src, uint32 src_increment,
		    uint32 src_x, uint32 src_y,
		    uint32 x, uint32 y, uint32 w, uint32 h)
{
   const uint8 *srcptr;
   uint32 *dstptr;
   int i, j, k;

   srcptr = src;
   srcptr += (src_y * src_increment) + (src_x * 3);

   line_increment >>= 2;
   dstptr = (uint32 *)tof;
   dstptr += (y * line_increment) + x;

   for (i=0; i<h; i++) {
      for (j=0, k=0; j<w; j++) {
	 uint8 blue = srcptr[k++];
	 uint8 green = srcptr[k++];
	 uint8 red = srcptr[k++];
	 dstptr[j] = (0xFF << 24) |
                     ((red << 16) & REDMASK_32) |
		     ((green << 8) & GREENMASK_32) |
		     (blue & BLUEMASK_32);
      }
      srcptr += src_increment;
      dstptr += line_increment;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterConvert32toShort --
 *
 *	Convert pixels from depth 32 to depth 15 or 16, while copying
 *	from source to destination.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
RasterConvert32toShort(uint8 *tof, uint32 line_increment,
		       const uint8 *src, uint32 src_increment,
		       uint32 src_x, uint32 src_y,
		       uint32 x, uint32 y, uint32 w, uint32 h,
		       uint32 redMask, uint32 greenMask, uint32 blueMask,
		       uint32 redShift, uint32 greenShift, uint32 blueShift)
{
   const uint32 *srcptr;
   uint16 *dstptr;
   int i, j;

   src_increment >>= 2;
   srcptr = (uint32 *)src;
   srcptr += (src_y * src_increment) + src_x;

   line_increment >>= 1;
   dstptr = (uint16 *)tof;
   dstptr += (y * line_increment) + x;

   for (i=0; i<h; i++) {
      for (j=0; j<w; j++) {
	 uint32 pix = srcptr[j];
	 dstptr[j] = (redMask & (pix >> redShift)) |
		     (greenMask & (pix >> greenShift)) |
		     (blueMask & (pix >> blueShift));
      }
      srcptr += src_increment;
      dstptr += line_increment;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterConvert32to24 --
 *
 *	Convert pixels from depth 32 to depth 24, while copying
 *	from source to destination.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
RasterConvert32to24(uint8 *tof, uint32 line_increment,
		    const uint8 *src, uint32 src_increment,
		    uint32 src_x, uint32 src_y,
		    uint32 x, uint32 y, uint32 w, uint32 h)
{
   const uint32 *srcptr;
   uint8 *dstptr;
   int i, j, k;

   src_increment >>= 2;
   srcptr = (uint32 *)src;
   srcptr += (src_y * src_increment) + src_x;

   dstptr = tof;
   dstptr += (y * line_increment) + (x * 3);

   for (i=0; i<h; i++) {
      for (j=0, k=0; j<w; j++) {
	 uint32 pix = srcptr[j];
	 dstptr[k++] = pix & BLUEMASK_32;
	 dstptr[k++] = (pix & GREENMASK_32) >> 8;
	 dstptr[k++] = (pix & REDMASK_32) >> 16;
      }
      srcptr += src_increment;
      dstptr += line_increment;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterConvertIndexto8 --
 *
 *	Convert pixels from pseudo color values to depth 8 true color while
 *	copying from source to destination.
 *	BGR233: redShift: 21 	greenShift: 10 	 blueShift: 0 	
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
RasterConvertIndexto8(uint8 *tof, uint32 line_increment,
			  const uint8 *src, uint32 src_increment, const uint32 *pixels,
			  uint32 src_x, uint32 src_y,
			  uint32 x, uint32 y, uint32 w, uint32 h,
			  uint32 redMask, uint32 greenMask, uint32 blueMask,
			  uint32 redShift, uint32 greenShift, uint32 blueShift)
{
   const uint8 *srcptr;
   uint8 *dstptr;
   int i, j;

   srcptr = src;
   srcptr += (src_y * src_increment) + src_x;

   dstptr = (uint8 *)tof;
   dstptr += (y * line_increment) + x;

   for (i=0; i<h; i++) {
      for (j=0; j<w; j++) {
	 uint32 pix = pixels[srcptr[j]];
	 dstptr[j] = (redMask & (pix >> redShift)) |
		     (greenMask & (pix >> greenShift)) |
		     (blueMask & (pix >> blueShift));
      }
      srcptr += src_increment;
      dstptr += line_increment;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterConvertIndextoShort --
 *
 *	Convert pixels from pseudo color values to depth 15 or 16, while
 *	copying from source to destination.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
RasterConvertIndextoShort(uint8 *tof, uint32 line_increment,
			  const uint8 *src, uint32 src_increment, const uint32 *pixels,
			  uint32 src_x, uint32 src_y,
			  uint32 x, uint32 y, uint32 w, uint32 h,
			  uint32 redMask, uint32 greenMask, uint32 blueMask,
			  uint32 redShift, uint32 greenShift, uint32 blueShift)
{
   const uint8 *srcptr;
   uint16 *dstptr;
   int i, j;

   srcptr = src;
   srcptr += (src_y * src_increment) + src_x;

   line_increment >>= 1;
   dstptr = (uint16 *)tof;
   dstptr += (y * line_increment) + x;

   for (i=0; i<h; i++) {
      for (j=0; j<w; j++) {
	 uint32 pix = pixels[srcptr[j]];
	 dstptr[j] = (redMask & (pix >> redShift)) |
		     (greenMask & (pix >> greenShift)) |
		     (blueMask & (pix >> blueShift));
      }
      srcptr += src_increment;
      dstptr += line_increment;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterConvertIndexto24 --
 *
 *	Convert pixels from pseudo color values to depth 24, while copying
 *	from source to destination.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
RasterConvertIndexto24(uint8 *tof, uint32 line_increment,
		       const uint8 *src, uint32 src_increment, const uint32 *pixels,
		       uint32 src_x, uint32 src_y,
		       uint32 x, uint32 y, uint32 w, uint32 h)
{
   const uint8 *srcptr;
   uint8 *dstptr;
   int i, j, k;

   srcptr = src;
   srcptr += (src_y * src_increment) + src_x;

   dstptr = tof;
   dstptr += (y * line_increment) + (x * 3);

   for (i=0; i<h; i++) {
      for (j=0, k=0; j<w; j++) {
	 uint32 pix = pixels[srcptr[j]];
	 dstptr[k++] = pix & BLUEMASK_32;
	 dstptr[k++] = (pix & GREENMASK_32) >> 8;
	 dstptr[k++] = (pix & REDMASK_32) >> 16;
      }
      srcptr += src_increment;
      dstptr += line_increment;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterConvertIndexto32 --
 *
 *	Convert pixels from pseudo color values to depth 32, while copying
 *	from source to destination.
 *
 * Results:
 *	None
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static void
RasterConvertIndexto32(uint8 *tof, uint32 line_increment,
		       const uint8 *src, uint32 src_increment, const uint32 *pixels,
		       uint32 src_x, uint32 src_y,
		       uint32 x, uint32 y, uint32 w, uint32 h)
{
   const uint8 *srcptr;
   uint32 *dstptr;
   int i, j;

   srcptr = src;
   srcptr += (src_y * src_increment) + src_x;

   line_increment >>= 2;
   dstptr = (uint32 *)tof;
   dstptr += (y * line_increment) + x;

   for (i=0; i<h; i++) {
      for (j=0; j<w; j++) {
	 uint32 pix = pixels[srcptr[j]];
	 dstptr[j] = pix;
      }
      srcptr += src_increment;
      dstptr += line_increment;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterConvert32to8 --
 *
 *	Convert pixels from depth 32 to depth 8, while copying
 *	from source to destination. 
 *	BGR233: redShift: 21 	greenShift: 10 	 blueShift: 0 
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
RasterConvert32to8(uint8 *tof, uint32 line_increment,
		   const uint8 *src, uint32 src_increment,
		   uint32 src_x, uint32 src_y,
		   uint32 x, uint32 y, uint32 w, uint32 h,
		   uint32 redMask, uint32 greenMask, uint32 blueMask,
		   uint32 redShift, uint32 greenShift, uint32 blueShift)
{
   const uint32 *srcptr;
   uint8 *dstptr;
   int i, j;

   src_increment >>= 2;
   srcptr = (uint32 *)src;
   srcptr += (src_y * src_increment) + src_x;

   dstptr = (uint8 *)tof;
   dstptr += (y * line_increment) + x;

   for (i=0; i<h; i++) {
      for (j=0; j<w; j++) {
	 uint32 pix = srcptr[j];
	 dstptr[j] = (redMask & (pix >> redShift)) |
		     (greenMask & (pix >> greenShift)) |
		     (blueMask & (pix >> blueShift));
      }
      srcptr += src_increment;
      dstptr += line_increment;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterConvert24to8 --
 *
 *	Convert pixels from depth 24 to depth 8, while copying
 *	from source to destination.
 *	BGR233: redShift: 5 	greenShift: 2 	 blueShift: 0 
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
RasterConvert24to8(uint8 *tof, uint32 line_increment,
		   const uint8 *src, uint32 src_increment,
		   uint32 src_x, uint32 src_y,
		   uint32 x, uint32 y, uint32 w, uint32 h,
		   uint32 redMask, uint32 greenMask, uint32 blueMask,
		   uint32 redShift, uint32 greenShift, uint32 blueShift)
{
   const uint8 *srcptr;
   uint8 *dstptr;
   int i, j, k;

   srcptr = src;
   srcptr += (src_y * src_increment) + (src_x * 3);

   dstptr = (uint8 *)tof;
   dstptr += (y * line_increment) + x;

   for (i=0; i<h; i++) {
      for (j=0, k=0; j<w; j++) {
	 uint8 blue = srcptr[k++];
	 uint8 green = srcptr[k++];
	 uint8 red = srcptr[k++];
	 dstptr[j] = ((red >> redShift) & redMask) |
		     ((green >> greenShift) & greenMask) |
		     ((blue >> blueShift) & blueMask);
      }
      srcptr += src_increment;
      dstptr += line_increment;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterConvert16to8 --
 *
 *	Convert pixels from depth 16/15 to depth 8 BGR, while copying from
 *	source to destination.
 *	For BGR233 and depth 16: redShift:13 greenShift:5 
 *				 blueShift:3 Shift left 
 *	For BGR233 and depth 15: redShift:12 greenShift:4 
 *				 blueShift:3 Shift left 
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
RasterConvert16to8(uint8 *tof, uint32 line_increment,
                   const uint8 *src, uint32 src_increment,
                   uint32 src_x, uint32 src_y,
                   uint32 x, uint32 y, uint32 w, uint32 h,
                   uint32 redMask, uint32 greenMask, uint32 blueMask,
                   int redShift, int greenShift, int blueShift)
{
   const uint16 *srcptr;
   uint8 *dstptr;
   int i, j;

   src_increment >>= 1;
   srcptr = (uint16 *)src;
   srcptr += (src_y * src_increment) + src_x;

   dstptr = (uint8 *)tof;
   dstptr += (y * line_increment) + x;

   for (i=0; i<h; i++) {
      for (j=0; j<w; j++) {
	 uint16 pix = srcptr[j];
	 dstptr[j] = (redMask & RasterShiftPixel(pix, redShift)) |
		     (greenMask & RasterShiftPixel(pix, greenShift)) |
		     (blueMask & RasterShiftPixel(pix, blueShift));
      }
      srcptr += src_increment;
      dstptr += line_increment;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * RasterGetShiftFromMask --
 *
 *      Caculate the shift from the mask. For example, if we want to
 *      convert from 24 bpp to BGR233, then for greenShift, the green mask 
 *      is 11100, green bits in 24 bpp starts from bit 16, the green bits in
 *      BGR233 starts from bit 6, then the shift is 16 - 6 = 10
 *
 * Results:
 *	The shift
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static int
RasterGetShiftFromMask(uint32 start, uint32 mask)
{
   uint32 num = 0;

   while (mask) {
      mask = mask >> 1;
      num++;
   }

   return (int)start - (int)num; 
}


/*
 *----------------------------------------------------------------------
 *
 * RasterShiftPixel --
 *
 *     Shift the pixel. If the shift is negative, shift to left,
 *     other, shift to right.
 *
 * Results:
 *	The shifted data
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

uint32
RasterShiftPixel(uint32 pixel, int shift)
{
   if (shift < 0) {
      return pixel << -shift;
   } else {
      return pixel >> shift;
   }
}
