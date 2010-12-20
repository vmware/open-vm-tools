/* **********************************************************
 * Copyright (C) 2002 VMware, Inc.
 * All Rights Reserved
 * **********************************************************/

/* $Xorg: regionstr.h,v 1.4 2001/02/09 02:05:15 xorgcvs Exp $ */
/***********************************************************

Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/* $XFree86: xc/programs/Xserver/include/regionstr.h,v 1.7 2001/12/14 19:59:56 dawes Exp $ */

/*
 * region.h --
 *
 *      Interface definition for miRegion operations. Originally from X11
 *      source distribution. This version includes modifications made for the
 *      VNC Reflector package.
 *
 * ChangeLog:
 *
 *	07/21/2003 rrdharan     - added miRegionArea function prototype
 *	                        - added REGION_AREA wrapper macro
 *	                        - added this header, removed whitespace
 *	08/02/2005 akan		- fixed conflicting definition of MINSHORT
 *				  and MAXSHORT with winnt.h
 *      04/03/2007 shelleygong  - use int instead of short for data
 *                                inside the region
 *      02/12/2010 michael - Since coordinates are kept as ints, coordinate values
 *      shouldn't be clamped to the range of short. I removed R_{MIN,MAX}SHORT
 *      and changed clamping to be in the range R_MININT..R_MAXINT instead.
 *      Since some code does "n < R_MININT" and "n > R_MAXINT", R_MININT must be
 *      greater than INT_MIN and R_MAXINT must be less than INT_MAX.
 *      03/08/2010 amarp        - Move xalloc macro outside the header because of
 *                                boost conflict with macro
 */

#ifndef __REGION_H__
#define __REGION_H__

#include "vmware.h"

/* Return values from RectIn() */
#define rgnOUT 0
#define rgnIN  1
#define rgnPART 2

#define NullBox ((BoxPtr)0)
#define NullRegion ((RegionPtr)0)

#define R_MAXINT 0x0FFFFFFF   /* Must be less than INT_MAX */
#define R_MININT 0x8FFFFFFF   /* Must be greater than INT_MIN */

#define CT_YXBANDED 18

#define xrealloc(ptr, n) realloc((ptr), (n))
#define xfree(ptr)       free(ptr)

typedef enum {
   UpdateRect,
   ROPFillRect,
   Present3dRect,
   LockRect,
   FenceRect,
   MaxRect
} RectInfoType;

typedef struct RectInfo {
   RectInfoType type;
   union {
      struct {
         uint32 rop;
         uint32 color;
      } ROPFill;
      struct {
         uint32 sid;
         uint32 srcx, srcy;
      } Present3d;
      struct {
         uint32 fenceId;
      } Fence;
      /* add more here, then update miRectInfosEqual and miPrintRegion. */
   };
} RectInfo, *RectInfoPtr;

#define RECTINFO_ISVALID(info) \
   ((info).type >= UpdateRect && (info).type < MaxRect)

/*
 * X data types
 */

typedef struct _Box {
   int x1, y1, x2, y2;
   RectInfo info;
} BoxRec, *BoxPtr;

typedef struct _xPoint {
    int x, y;
} xPoint, *xPointPtr;

typedef xPoint DDXPointRec, *DDXPointPtr;

typedef struct _xRectangle {
    int x, y;
    unsigned short width, height;
    RectInfo info;
} xRectangle, *xRectanglePtr;

/*
 *   clip region
 */

typedef struct _RegData {
    int		size;
    int		numRects;
/*  BoxRec	rects[size];   in memory but not explicitly declared */
} RegDataRec, *RegDataPtr;

typedef struct _Region {
    BoxRec	extents;
    RegDataPtr	data;
} RegionRec, *RegionPtr;

extern BoxRec miEmptyBox;
extern RegDataRec miEmptyData;
extern RegDataRec miBrokenData;

#define REGION_EXTENTS(reg) (&(reg)->extents)
#define REGION_NIL(reg) ((reg)->data && !(reg)->data->numRects)
#define REGION_NAR(reg)	((reg)->data == &miBrokenData)
#define REGION_NUM_RECTS(reg) ((reg)->data ? (reg)->data->numRects : 1)
#define REGION_SIZE(reg) ((reg)->data ? (reg)->data->size : 0)
#define REGION_RECTS(reg) ((reg)->data ? (BoxPtr)((reg)->data + 1) \
			               : &(reg)->extents)
#define REGION_BOXPTR(reg) ((BoxPtr)((reg)->data + 1))
#define REGION_BOX(reg,i) (&REGION_BOXPTR(reg)[i])
#define REGION_TOP(reg) REGION_BOX(reg, (reg)->data->numRects)
#define REGION_END(reg) REGION_BOX(reg, (reg)->data->numRects - 1)
#define REGION_SZOF(n) (sizeof(RegDataRec) + ((n) * sizeof(BoxRec)))

#define REGION_VALIDINDEX(reg, i) (i >= 0 && i < REGION_NUM_RECTS((reg)))

#define BOX_X(ptr) (ptr->x1)
#define BOX_Y(ptr) (ptr->y1)
#define BOX_WIDTH(ptr) (ptr->x2 - ptr->x1)
#define BOX_HEIGHT(ptr) (ptr->y2 - ptr->y1)
#define BOX_XY(ptr) BOX_X(ptr), BOX_Y(ptr)
#define BOX_WH(ptr) BOX_WIDTH(ptr), BOX_HEIGHT(ptr)
#define BOX_XYWH(ptr) BOX_XY(ptr), BOX_WH(ptr)

#define RECT_SETBOX(r, rx, ry, rw, rh) do { \
  (r)->x1 = (rx); \
  (r)->x2 = (rx) + (rw); \
  (r)->y1 = (ry); \
  (r)->y2 = (ry) + (rh); \
  (r)->info.type = UpdateRect; \
} while (FALSE)

#define RECT_SETRECT(r, rx1, ry1, rx2, ry2) do { \
  RECT_SETBOX((r), (rx1), (ry1), ((rx2) - (rx1)), ((ry2) - (ry1))); \
} while (FALSE)

#define RECT_SETVMRECT(r, vmr) \
  RECT_SETRECT((r), (vmr)->left,  (vmr)->top,  (vmr)->right,  (vmr)->bottom)

/*
 * This will only work if the intersection is not empty.
 */
#define RECT_INTERSECT(r, r1, r2) \
{ \
   (r)->x1 = MAX((r1)->x1, (r2)->x1); \
   (r)->x2 = MIN((r1)->x2, (r2)->x2); \
   (r)->y1 = MAX((r1)->y1, (r2)->y1); \
   (r)->y2 = MIN((r1)->y2, (r2)->y2); \
}

/*
 *  True iff the two boxes overlap.
 */
#define RECT_EXTENTCHECK(r1, r2) \
   (!( ((r1)->x2 <= (r2)->x1)  || \
       ((r1)->x1 >= (r2)->x2)  || \
       ((r1)->y2 <= (r2)->y1)  || \
       ((r1)->y1 >= (r2)->y2) ) )

/*
 * True iff both the BoxRecs are identical.
 */
#define RECT_IDENTICAL(r1, r2) \
   (( ((r1)->x1 == (r2)->x1)  && \
      ((r1)->x2 == (r2)->x2)  && \
      ((r1)->y1 == (r2)->y1)  && \
      ((r1)->y2 == (r2)->y2) ) )


extern RegionPtr miRegionCreate(BoxPtr rect, int size);
extern void miRegionInit(RegionPtr pReg, BoxPtr rect, int size);
extern void miRegionDestroy(RegionPtr pReg);
extern void miRegionUninit(RegionPtr pReg);
extern Bool miRegionCopy(RegionPtr dst, RegionPtr src);
extern Bool miIntersect(RegionPtr newReg, RegionPtr reg1, RegionPtr reg2);
extern Bool miUnion(RegionPtr newReg, RegionPtr reg1, RegionPtr reg2);
extern Bool miRegionAppend(RegionPtr dstrgn, RegionPtr rgn);
extern Bool miRegionValidate(RegionPtr badreg, Bool *pOverlap);
extern RegionPtr miRectsToRegion(int nrects, xRectanglePtr prect, int ctype);
extern RegionPtr miRectsToRegionbyBoundary(int nrects, xRectanglePtr prect, int ctype,
                                           int minValue, int maxValue);
extern Bool miSubtract(RegionPtr regD, RegionPtr regM, RegionPtr regS);
extern Bool miInverse(RegionPtr newReg, RegionPtr reg1, BoxPtr invRect);
extern int miRectIn(RegionPtr region, BoxPtr prect);
extern void miTranslateRegion(RegionPtr pReg, int x, int y);
extern void miTranslateRegionByBoundary(RegionPtr pReg, int x, int y,
                                        int minValue, int maxValue);
extern void miRegionReset(RegionPtr pReg, BoxPtr pBox);
extern Bool miRegionBreak(RegionPtr pReg);
extern Bool miPointInRegion(RegionPtr pReg, int x, int y, BoxPtr box);
extern Bool miRegionNotEmpty(RegionPtr pReg);
extern void miRegionEmpty(RegionPtr pReg);
extern Bool miRegionsEqual(RegionPtr reg1, RegionPtr reg2);
extern BoxPtr miRegionExtents(RegionPtr pReg);
extern int miRegionArea(RegionPtr pReg);
extern void miRegionPack(RegionPtr pReg, int threshold);

extern Bool miApplyRect(RegionPtr newReg, RegionPtr reg, BoxPtr rect,
                        Bool (*op) (RegionPtr, RegionPtr, RegionPtr));
extern int miPrintRegion(RegionPtr rgn);

typedef Bool (*miRegionMatchFunc)(BoxPtr box, uintptr_t userData);
extern Bool miRegionMatch(RegionPtr newReg, RegionPtr reg,
                          miRegionMatchFunc match, uintptr_t userData);

// Extension to REGION_NIL that also checks for a 0 x 0 bounding rect.
extern Bool miIsRegionVoid(RegionPtr pReg);

#endif /* __REGION_H__ */
