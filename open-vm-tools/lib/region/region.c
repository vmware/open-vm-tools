/* **********************************************************
 * Copyright (C) 2002 VMware, Inc.
 * All Rights Reserved
 * **********************************************************/

/* $XFree86: xc/programs/Xserver/mi/miregion.c,v 1.8 2001/12/14 20:00:26 dawes Exp $ */
/***********************************************************

Copyright 1987, 1988, 1989, 1998  The Open Group

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
 

Copyright 1987, 1988, 1989 by 
Digital Equipment Corporation, Maynard, Massachusetts. 

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
/* $Xorg: miregion.c,v 1.4 2001/02/09 02:05:21 xorgcvs Exp $ */

/* The panoramix components contained the following notice */
/*****************************************************************

Copyright (c) 1991, 1997 Digital Equipment Corporation, Maynard, Massachusetts.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
DIGITAL EQUIPMENT CORPORATION BE LIABLE FOR ANY CLAIM, DAMAGES, INCLUDING,
BUT NOT LIMITED TO CONSEQUENTIAL OR INCIDENTAL DAMAGES, OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Digital Equipment Corporation
shall not be used in advertising or otherwise to promote the sale, use or other
dealings in this Software without prior written authorization from Digital
Equipment Corporation.

******************************************************************/

/*
 * miregion.c --
 *
 *	This is a slightly modified copy of miregion.c from the XFree86 4.3.0
 *	distribution: See http://www.xfree86.org, and the file
 *	xc/programs/Xserver/mi/miregion.c.
 *
 * ChangeLog:
 *
 *	07/10/2003 rrdharan	- removed X11 dependencies
 *			        - added prototype declarations for
 *			          miSetExtents and miFindMaxBands to prevent
 *			          compilation warnings
 *			        - #define assert ASSERT
 *			        - added FILECODE and comments
 *	09/01/2003 rrdharan     - added initial support for accelerated
 *	                          operations: all rectangles now have a
 *	                          RectInfo struct associated with them that
 *	                          contains metadata for the rectangle. See the
 *	                          explanation of extensions below.
 *	08/02/2005 akan		- fixed conflicting definition of MINSHORT
 *				  and MAXSHORT with winnt.h
 *      04/03/2007 shelleygong  - use int instead of short for data
 *                                inside the region *
 * 02/12/2010 michael - Since coordinates are kept as ints, coordinate values
 *    shouldn't be clamped to the range of short. I removed R_{MIN,MAX}SHORT
 *    and changed clamping to be in the range R_MININT..R_MAXINT instead.
 *    Since some code does "n < R_MININT" and "n > R_MAXINT", R_MININT must be
 *    greater than INT_MIN and R_MAXINT must be less than INT_MAX.
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "region.h"

void miSetExtents(RegionPtr pReg);
int miFindMaxBand(RegionPtr prgn);

#define good(reg) ASSERT(miValidRegion(reg))

/*
 * The functions in this file implement the Region abstraction used extensively
 * throughout the X11 sample server. A Region is simply a set of disjoint
 * (non-overlapping) rectangles, plus an "extent" rectangle which is the
 * smallest single rectangle that contains all the non-overlapping rectangles.
 *
 * A Region is implemented as a "y-x-banded" array of rectangles.  This array
 * imposes two degrees of order.  First, all rectangles are sorted by top side
 * y coordinate first (y1), and then by left side x coordinate (x1).
 *
 * Furthermore, the rectangles are grouped into "bands".  Each rectangle in a
 * band has the same top y coordinate (y1), and each has the same bottom y
 * coordinate (y2).  Thus all rectangles in a band differ only in their left
 * and right side (x1 and x2).  Bands are implicit in the array of rectangles:
 * there is no separate list of band start pointers.
 *
 * The y-x band representation does not minimize rectangles.  In particular,
 * if a rectangle vertically crosses a band (the rectangle has scanlines in 
 * the y1 to y2 area spanned by the band), then the rectangle may be broken
 * down into two or more smaller rectangles stacked one atop the other. 
 *
 *  -----------                              -----------
 *  |         |                              |         |             band 0
 *  |         |  --------                    -----------  --------
 *  |         |  |      |  in y-x banded     |         |  |      |   band 1
 *  |         |  |      |  form is           |         |  |      |
 *  -----------  |      |                    -----------  --------
 *               |      |                                 |      |   band 2
 *               --------                                 --------
 *
 * An added constraint on the rectangles is that they must cover as much
 * horizontal area as possible: no two rectangles within a band are allowed
 * to touch.
 *
 * Whenever possible, bands will be merged together to cover a greater vertical
 * distance (and thus reduce the number of rectangles). Two bands can be merged
 * only if the bottom of one touches the top of the other and they have
 * rectangles in the same places (of the same width, of course).
 *
 * Adam de Boor wrote most of the original region code.  Joel McCormack
 * substantially modified or rewrote most of the core arithmetic routines,
 * and added miRegionValidate in order to support several speed improvements
 * to miValidateTree.  Bob Scheifler changed the representation to be more
 * compact when empty or a single rectangle, and did a bunch of gratuitous
 * reformatting.
 */

/*
 * Accelerated Operation Support (09/03/03, rrdharan):
 *
 *      We've extended the region code to support regions consisting of a mix
 *      of update rectangles and "accelerated" operations (e.g. Fill).  The
 *      way it works is pretty straightforward, and the behavior should be
 *      identical to the original region code when only update rectangles are
 *      involved.
 *
 *      Each rectangle now has a RectInfo structure member; this structure
 *      indicates the type of the rectangle and carries any relevant metadata,
 *      e.g. for a Fill, the rop and color. For each region operator, e.g.
 *      miUnion, miIntersect, we treat the two source operands as layers,
 *      where the first operand is the bottom layer and the second is the top.
 *      Operations in the top layer stencil out the bottom layer. Note that we
 *      still maintain the fundamental banding property of the regions.
 *
 *      For clarification, consider the example below, where region 1 (the
 *      bottom layer) consists of two updates and region 2 (the top layer)
 *      consists of two fills. The union of the two regions results in a new
 *      region where the fill operations have been stencilled into the
 *      previous update rectangles, producing a new list of updates and fills.
 *      The chief advantage of this approach is that it provides
 *      order-independence; i.e. after any region operation has completed,
 *      we can scan out the contents of the region and mapp the rectangles
 *      onto their corresponding drawing operations in any order and still end
 *      up with the correct result.
 *
 *      Note that because we still enforce the banding rules, we pass up
 *      opportunities for further optimization, (see comments on this in the
 *      original documentation above).
 *
 *      Region 1 (bottom):                          Region 2 (top):
 *
 *      -----------------------                 -----------------------
 *      |------------         |                 |                     |
 *      ||U         |         |                 |                     |
 *      ||          |         |                 |   --------------    |
 *      |------------         |                 |   |F           |    |
 *      |                     |                 |   |            |    |
 *      |                     |     (union)     |   |            |    |
 *      |        -------------|                 |   --------------    |
 *      |        |U          ||                 |                     |
 *      |        |           ||                 |              ------ |
 *      |        |           ||                 |              |F   | |
 *      |        -------------|                 |              ------ |
 *      -----------------------                 -----------------------
 *
 *
 *                              =  New Region:
 *
 *                          -----------------------
 *                          |------------         |
 *                          ||U         |         |
 *                          ||----------------    |
 *                          ||U |F           |    |
 *                          |-----------------    |
 *                          |   |F           |    |
 *                          |   --------------    |
 *                          |        -------------|
 *                          |        |U          ||
 *                          |        |-----------||
 *                          |        |U    |F   |||
 *                          |        -------------|
 *                          -----------------------
 *
 */

/*  true iff two Boxes overlap */
#define EXTENTCHECK RECT_EXTENTCHECK

/* true iff (x,y) is in Box */
#define INBOX(r,x,y) \
      ( ((r)->x2 >  x) && \
        ((r)->x1 <= x) && \
        ((r)->y2 >  y) && \
        ((r)->y1 <= y) )

/* true iff Box r1 contains Box r2 */
#define SUBSUMES(r1,r2) \
      ( ((r1)->x1 <= (r2)->x1) && \
        ((r1)->x2 >= (r2)->x2) && \
        ((r1)->y1 <= (r2)->y1) && \
        ((r1)->y2 >= (r2)->y2) )

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define xallocData(n)    (RegDataPtr)malloc(REGION_SZOF(n))
#define xfreeData(reg)   if ((reg)->data && (reg)->data->size) \
                           free((reg)->data)

#define RECTALLOC_BAIL(pReg,n,bail) \
if (!(pReg)->data || (((pReg)->data->numRects + (n)) > (pReg)->data->size)) \
    if (!miRectAlloc(pReg, n)) { goto bail; }

#define RECTALLOC(pReg,n) \
if (!(pReg)->data || (((pReg)->data->numRects + (n)) > (pReg)->data->size)) \
    if (!miRectAlloc(pReg, n)) { return FALSE; }

#define ADDRECT(pNextRect,nx1,ny1,nx2,ny2,inf)	\
{						\
    pNextRect->x1 = nx1;			\
    pNextRect->y1 = ny1;			\
    pNextRect->x2 = nx2;			\
    pNextRect->y2 = ny2;			\
    pNextRect->info = inf;                      \
    pNextRect++;				\
}

#define NEWRECT(pReg,pNextRect,nx1,ny1,nx2,ny2,inf)			\
{									\
    if (!(pReg)->data || ((pReg)->data->numRects == (pReg)->data->size))\
    {									\
	if (!miRectAlloc(pReg, 1))					\
	    return FALSE;						\
	pNextRect = REGION_TOP(pReg);					\
    }									\
    ADDRECT(pNextRect,nx1,ny1,nx2,ny2,inf);			        \
    pReg->data->numRects++;						\
    ASSERT(pReg->data->numRects<=pReg->data->size);			\
}


#define DOWNSIZE(reg,numRects)						 \
if (((numRects) < ((reg)->data->size >> 1)) && ((reg)->data->size > 50)) \
{									 \
    RegDataPtr NewData;							 \
    NewData = (RegDataPtr)xrealloc((reg)->data, REGION_SZOF(numRects));	 \
    if (NewData)							 \
    {									 \
	NewData->size = (numRects);					 \
	(reg)->data = NewData;						 \
    }									 \
}

BoxRec miEmptyBox = {0, 0, 0, 0};
RegDataRec miEmptyData = {0, 0};

RegDataRec  miBrokenData = {0, 0};
RegionRec   miBrokenRegion = { { 0, 0, 0, 0 }, &miBrokenData };


int
miPrintRegion(RegionPtr rgn)
{
    int num, size;
    register int i;
    BoxPtr rects;

    num = REGION_NUM_RECTS(rgn);
    size = REGION_SIZE(rgn);
    rects = REGION_RECTS(rgn);
    Warning("num: %d size: %d\n", num, size);
    Warning("extents: %d %d %d %d\n",
	   rgn->extents.x1, rgn->extents.y1, rgn->extents.x2, rgn->extents.y2);
    for (i = 0; i < num; i++) {
      Warning("%4d %4d %4d %4d",
	     rects[i].x1, rects[i].y1, rects[i].x2, rects[i].y2);
      switch(rects[i].info.type) {
      case UpdateRect:
         Warning(" UpdateRect ");
         break;
      case ROPFillRect:
         Warning(" ROPFillRect   0x%08x", rects[i].info.ROPFill.color);
         break;
      case Present3dRect:
         Warning(" Present3DRect   0x%x   src(%d, %d)",
                 rects[i].info.Present3d.sid,
                 rects[i].info.Present3d.srcx, rects[i].info.Present3d.srcy);
         break;
      case LockRect:
         Warning(" LockRect ");
      case FenceRect:
         Warning(" FenceRect fence: 0x%x", rects[i].info.Fence.fenceId);
      default:
         Warning(" UNKNOWN!");
         break;
      }
      Warning("\n");
    }
    Warning("\n");
    return(num);
}


/*
 *----------------------------------------------------------------------------
 *
 * miRectInfosEqual --
 *
 *      Takes pointers to two RectInfo structures and compares them for
 *      equality based on their type and data: e.g. updates are always equal,
 *      fills are equal only if the rop and color match.
 *
 * Results:
 *      TRUE if the two RectInfo structures match, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE Bool
miRectInfosEqual(RectInfo *info1,
                 RectInfo *info2)
{
   if (info1->type != info2->type) {
      return FALSE;
   }

   switch (info1->type) {
   case UpdateRect:
   case LockRect:
      return TRUE;
   case ROPFillRect:
      return (info1->ROPFill.rop == info2->ROPFill.rop &&
              info1->ROPFill.color == info2->ROPFill.color);
   case Present3dRect:
      return (info1->Present3d.sid == info2->Present3d.sid &&
              info1->Present3d.srcx == info2->Present3d.srcx &&
              info1->Present3d.srcy == info2->Present3d.srcy);
   case FenceRect:
      return (info1->Fence.fenceId == info2->Fence.fenceId);
   default:
      NOT_IMPLEMENTED();
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * miRectInfosVertAdjacent --
 *
 *      Takes pointers to two RectInfo structures of type Present3dRect and
 *      decides whether they're vertically mergeable (same sid, same srcx,
 *      srcy adjacent).
 *
 * Results:
 *      TRUE if the two RectInfo structures are adjacent, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE Bool
miRectInfosVertAdjacent(RectInfo *info1,
                        int y1,
                        RectInfo *info2,
                        int y2)
{
   if (info1->type == Present3dRect && info2->type == Present3dRect) {
      return (info1->Present3d.sid == info2->Present3d.sid &&
              info1->Present3d.srcx == info2->Present3d.srcx &&
              info1->Present3d.srcy - y1 == info2->Present3d.srcy - y2);
   } else {
      return miRectInfosEqual(info1, info2);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * miRectInfosHorzAdjacent --
 *
 *      Takes pointers to two RectInfo structures of type Present3dRect
 *      and decides whether they're horizontally mergeable (same sid,
 *      same srcy, srcx adjacent).
 *
 * Results:
 *      TRUE if the two RectInfo structures are adjacent, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE Bool
miRectInfosHorzAdjacent(RectInfo *info1,
                        int x1,
                        RectInfo *info2,
                        int x2)
{
   if (info1->type == Present3dRect && info2->type == Present3dRect) {
      return (info1->Present3d.sid == info2->Present3d.sid &&
              info1->Present3d.srcx - x1 == info2->Present3d.srcx - x2 &&
              info1->Present3d.srcy == info2->Present3d.srcy);
   } else {
      return miRectInfosEqual(info1, info2);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * miRectInfoAdjustForSplit --
 *
 *      Adjusts the type-specific data in the RectInfo to be correct
 *      for a split that moves the upper left corner of the associated
 *      rectangle from (oldX, oldY) to (newX, newY).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Fiddles with *info as described above.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
miRectInfoAdjustForSplit(RectInfo *info,
                         int oldX,
                         int oldY,
                         int newX,
                         int newY)
{
   ASSERT(newX >= oldX);
   ASSERT(newY >= oldY);

   if (info->type == Present3dRect) {
      info->Present3d.srcx += newX - oldX;
      info->Present3d.srcy += newY - oldY;
   }
}


Bool
miRegionsEqual(RegionPtr reg1,
               RegionPtr reg2)
{
    int i;
    BoxPtr rects1, rects2;

    if (reg1->extents.x1 != reg2->extents.x1) return FALSE;
    if (reg1->extents.x2 != reg2->extents.x2) return FALSE;
    if (reg1->extents.y1 != reg2->extents.y1) return FALSE;
    if (reg1->extents.y2 != reg2->extents.y2) return FALSE;
    if (REGION_NUM_RECTS(reg1) != REGION_NUM_RECTS(reg2)) return FALSE;
    
    rects1 = REGION_RECTS(reg1);
    rects2 = REGION_RECTS(reg2);
    for (i = 0; i != REGION_NUM_RECTS(reg1); i++) {
	if (rects1[i].x1 != rects2[i].x1) return FALSE;
	if (rects1[i].x2 != rects2[i].x2) return FALSE;
	if (rects1[i].y1 != rects2[i].y1) return FALSE;
	if (rects1[i].y2 != rects2[i].y2) return FALSE;
        if (!miRectInfosEqual(&rects1[i].info, &rects2[i].info)) return FALSE;
    }
    return TRUE;
}


Bool
miValidRegion(RegionPtr reg)
{
    register int i, numRects;

    if ((reg->extents.x1 > reg->extents.x2) ||
	(reg->extents.y1 > reg->extents.y2))
	return FALSE;
    numRects = REGION_NUM_RECTS(reg);
    if (!numRects)
	return ((reg->extents.x1 == reg->extents.x2) &&
		(reg->extents.y1 == reg->extents.y2) &&
		(reg->data->size || (reg->data == &miEmptyData)));
    else if (numRects == 1)
	return (RECTINFO_ISVALID((reg->extents.info)) && !reg->data);
    else
    {
	register BoxPtr pboxP, pboxN;
	BoxRec box;

	pboxP = REGION_RECTS(reg);
	box = *pboxP;
	box.y2 = pboxP[numRects-1].y2;
	pboxN = pboxP + 1;
	for (i = numRects; --i > 0; pboxP++, pboxN++)
	{
            if (!RECTINFO_ISVALID(pboxP->info)) {
               return FALSE;
            }
	    if ((pboxN->x1 >= pboxN->x2) ||
		(pboxN->y1 >= pboxN->y2))
		return FALSE;
	    if (pboxN->x1 < box.x1)
	        box.x1 = pboxN->x1;
	    if (pboxN->x2 > box.x2)
		box.x2 = pboxN->x2;
	    if ((pboxN->y1 < pboxP->y1) ||
		((pboxN->y1 == pboxP->y1) &&
		 ((pboxN->x1 < pboxP->x2) || (pboxN->y2 != pboxP->y2))))
		return FALSE;
	}
	return ((box.x1 == reg->extents.x1) &&
		(box.x2 == reg->extents.x2) &&
		(box.y1 == reg->extents.y1) &&
		(box.y2 == reg->extents.y2));
    }
}


/*****************************************************************
 *   RegionCreate(rect, size)
 *     This routine does a simple malloc to make a structure of
 *     REGION of "size" number of rectangles.
 *****************************************************************/

RegionPtr
miRegionCreate(rect, size)
    BoxPtr rect;
    int size;
{
    register RegionPtr pReg;
   
    pReg = (RegionPtr)xalloc(sizeof(RegionRec));
    if (!pReg)
	return &miBrokenRegion;
    if (rect)
    {
	pReg->extents = *rect;
	pReg->data = (RegDataPtr)NULL;
    }
    else
    {
	pReg->extents = miEmptyBox;
	if ((size > 1) && (pReg->data = xallocData(size)))
	{
	    pReg->data->size = size;
	    pReg->data->numRects = 0;
	}
	else
	    pReg->data = &miEmptyData;
    }
    return(pReg);
}

/*****************************************************************
 *   RegionInit(pReg, rect, size)
 *     Outer region rect is statically allocated.
 *****************************************************************/

void
miRegionInit(pReg, rect, size)
    RegionPtr pReg;
    BoxPtr rect;
    int size;
{
    if (rect)
    {
	pReg->extents = *rect;
	pReg->data = (RegDataPtr)NULL;
    }
    else
    {
	pReg->extents = miEmptyBox;
	if ((size > 1) && (pReg->data = xallocData(size)))
	{
	    pReg->data->size = size;
	    pReg->data->numRects = 0;
	}
	else
	    pReg->data = &miEmptyData;
    }
}

void
miRegionDestroy(pReg)
    RegionPtr pReg;
{
    good(pReg);
    xfreeData(pReg);
    if (pReg != &miBrokenRegion)
	xfree(pReg);
}

void
miRegionUninit(pReg)
    RegionPtr pReg;
{
    good(pReg);
    xfreeData(pReg);
}

Bool
miRegionBreak (pReg)
    RegionPtr pReg;
{
    xfreeData (pReg);
    pReg->extents = miEmptyBox;
    pReg->data = &miBrokenData;
    return FALSE;
}

Bool
miRectAlloc(
    register RegionPtr pRgn,
    int n)
{
    RegDataPtr	data;
    
    if (!pRgn->data)
    {
	n++;
	pRgn->data = xallocData(n);
	if (!pRgn->data)
	    return miRegionBreak (pRgn);
	pRgn->data->numRects = 1;
	*REGION_BOXPTR(pRgn) = pRgn->extents;
    }
    else if (!pRgn->data->size)
    {
	pRgn->data = xallocData(n);
	if (!pRgn->data)
	    return miRegionBreak (pRgn);
	pRgn->data->numRects = 0;
    }
    else
    {
	if (n == 1)
	{
	    n = pRgn->data->numRects;
	    if (n > 500) /* XXX pick numbers out of a hat */
		n = 250;
	}
	n += pRgn->data->numRects;
	data = (RegDataPtr)xrealloc(pRgn->data, REGION_SZOF(n));
	if (!data)
	    return miRegionBreak (pRgn);
	pRgn->data = data;
    }
    pRgn->data->size = n;
    return TRUE;
}

Bool
miRegionCopy(dst, src)
    register RegionPtr dst;
    register RegionPtr src;
{
    good(dst);
    good(src);
    if (dst == src)
	return TRUE;
    dst->extents = src->extents;
    if (!src->data || !src->data->size)
    {
	xfreeData(dst);
	dst->data = src->data;
	return TRUE;
    }
    if (!dst->data || (dst->data->size < src->data->numRects))
    {
	xfreeData(dst);
	dst->data = xallocData(src->data->numRects);
	if (!dst->data)
	    return miRegionBreak (dst);
	dst->data->size = src->data->numRects;
    }
    dst->data->numRects = src->data->numRects;
    memmove((char *)REGION_BOXPTR(dst),(char *)REGION_BOXPTR(src), 
	  dst->data->numRects * sizeof(BoxRec));
    return TRUE;
}


/*======================================================================
 *	    Generic Region Operator
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miCoalesce --
 *	Attempt to merge the boxes in the current band with those in the
 *	previous one.  We are guaranteed that the current band extends to
 *      the end of the rects array.  Used only by miRegionOp.
 *
 * Results:
 *	The new index for the previous band.
 *
 * Side Effects:
 *	If coalescing takes place:
 *	    - rectangles in the previous band will have their y2 fields
 *	      altered.
 *	    - pReg->data->numRects will be decreased.
 *
 *-----------------------------------------------------------------------
 */
INLINE static int
miCoalesce (
    register RegionPtr	pReg,	    	/* Region to coalesce		     */
    int	    	  	prevStart,  	/* Index of start of previous band   */
    int	    	  	curStart)   	/* Index of start of current band    */
{
    register BoxPtr	pPrevBox;   	/* Current box in previous band	     */
    register BoxPtr	pCurBox;    	/* Current box in current band       */
    register int  	numRects;	/* Number rectangles in both bands   */
    register int	y2;		/* Bottom of current band	     */
    /*
     * Figure out how many rectangles are in the band.
     */
    numRects = curStart - prevStart;
    ASSERT(numRects == pReg->data->numRects - curStart);

    if (!numRects) return curStart;

    /*
     * The bands may only be coalesced if the bottom of the previous
     * matches the top scanline of the current.
     */
    pPrevBox = REGION_BOX(pReg, prevStart);
    pCurBox = REGION_BOX(pReg, curStart);
    if (pPrevBox->y2 != pCurBox->y1) return curStart;

    /*
     * Make sure the bands have boxes in the same places. This
     * assumes that boxes have been added in such a way that they
     * cover the most area possible. I.e. two boxes in a band must
     * have some horizontal space between them.
     */
    y2 = pCurBox->y2;

    do {
	if ((pPrevBox->x1 != pCurBox->x1) || (pPrevBox->x2 != pCurBox->x2)) {
	    return (curStart);
	}

        if (!(miRectInfosVertAdjacent(&pPrevBox->info, pPrevBox->y1,
                                      &pCurBox->info, pCurBox->y1))) {
           return (curStart);
        }

	pPrevBox++;
	pCurBox++;
	numRects--;
    } while (numRects);

    /*
     * The bands may be merged, so set the bottom y of each box
     * in the previous band to the bottom y of the current band.
     */
    numRects = curStart - prevStart;
    pReg->data->numRects -= numRects;
    do {
	pPrevBox--;
	pPrevBox->y2 = y2;
	numRects--;
    } while (numRects);
    return prevStart;
}


/* Quicky macro to avoid trivial reject procedure calls to miCoalesce */

#define Coalesce(newReg, prevBand, curBand)				\
    if (curBand - prevBand == newReg->data->numRects - curBand) {	\
	prevBand = miCoalesce(newReg, prevBand, curBand);		\
    } else {								\
	prevBand = curBand;						\
    }

/*-
 *-----------------------------------------------------------------------
 * miAppendNonO --
 *	Handle a non-overlapping band for the union and subtract operations.
 *      Just adds the (top/bottom-clipped) rectangles into the region.
 *      Doesn't have to check for subsumption or anything.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	pReg->data->numRects is incremented and the rectangles overwritten
 *	with the rectangles we're passed.
 *
 *-----------------------------------------------------------------------
 */

INLINE static Bool
miAppendNonO (
    register RegionPtr	pReg,
    register BoxPtr	r,
    BoxPtr  	  	rEnd,
    register int  	y1,
    register int  	y2)
{
    register BoxPtr	pNextRect;
    register int	newRects;

    newRects = rEnd - r;

    ASSERT(y1 < y2);
    ASSERT(newRects != 0);

    /* Make sure we have enough space for all rectangles to be added */
    RECTALLOC(pReg, newRects);
    pNextRect = REGION_TOP(pReg);
    pReg->data->numRects += newRects;
    do {
        RectInfo info = r->info;
	ASSERT(r->x1 < r->x2);
        miRectInfoAdjustForSplit(&info, 0, r->y1, 0, y1);
	ADDRECT(pNextRect, r->x1, y1, r->x2, y2, info);
	r++;
    } while (r != rEnd);

    return TRUE;
}

#define FindBand(r, rBandEnd, rEnd, ry1)		    \
{							    \
    ry1 = r->y1;					    \
    rBandEnd = r+1;					    \
    while ((rBandEnd != rEnd) && (rBandEnd->y1 == ry1)) {   \
	rBandEnd++;					    \
    }							    \
}

#define	AppendRegions(newReg, r, rEnd)					\
{									\
    int newRects;							\
    if ((newRects = rEnd - r)) {					\
	RECTALLOC(newReg, newRects);					\
	memmove((char *)REGION_TOP(newReg),(char *)r, 			\
              newRects * sizeof(BoxRec));				\
	newReg->data->numRects += newRects;				\
    }									\
}

/*-
 *-----------------------------------------------------------------------
 * miRegionOp --
 *	Apply an operation to two regions. Called by miUnion, miInverse,
 *	miSubtract, miIntersect....  Both regions MUST have at least one
 *      rectangle, and cannot be the same object.
 *
 * Results:
 *	TRUE if successful.
 *
 * Side Effects:
 *	The new region is overwritten.
 *	pOverlap set to TRUE if overlapFunc ever returns TRUE.
 *
 * Notes:
 *	The idea behind this function is to view the two regions as sets.
 *	Together they cover a rectangle of area that this function divides
 *	into horizontal bands where points are covered only by one region
 *	or by both. For the first case, miAppendNonO is called with
 *	each the band and the band's upper and lower extents. For the
 *	second, the overlapFunc is called to process the entire band. It
 *	is responsible for clipping the rectangles in the band, though
 *	this function provides the boundaries.
 *	At the end of each band, the new region is coalesced, if possible,
 *	to reduce the number of rectangles in the region.
 *
 *-----------------------------------------------------------------------
 */

typedef Bool (*OverlapProcPtr)(
    RegionPtr	pReg,
    BoxPtr	r1,
    BoxPtr   	r1End,
    BoxPtr	r2,
    BoxPtr   	r2End,
    int    	y1,
    int    	y2,
    Bool	*pOverlap);

static Bool miIntersectO(RegionPtr pReg, BoxPtr r1, BoxPtr r1End, BoxPtr r2,
                        BoxPtr r2End, int y1, int y2, Bool *pOverlap);
static Bool miUnionO(RegionPtr pReg, BoxPtr r1, BoxPtr r1End, BoxPtr r2,
                        BoxPtr r2End, int y1, int y2, Bool *pOverlap);
static Bool miSubtractO(RegionPtr pReg, BoxPtr r1, BoxPtr r1End, BoxPtr	r2,
                        BoxPtr r2End, int y1, int y2, Bool *pOverlap);

static Bool
miRegionOp(
    RegionPtr       newReg,		    /* Place to store result	     */
    RegionPtr       reg1,		    /* First region in operation     */
    RegionPtr       reg2,		    /* 2d region in operation        */
    OverlapProcPtr  overlapFunc,            /* Function to call for over-
					     * lapping bands		     */
    Bool	    appendNon1,		    /* Append non-overlapping bands  */
					    /* in region 1 ? */
    Bool	    appendNon2,		    /* Append non-overlapping bands  */
					    /* in region 2 ? */
    Bool	    *pOverlap)
{
    register BoxPtr r1;			    /* Pointer into first region     */
    register BoxPtr r2;			    /* Pointer into 2d region	     */
    BoxPtr	    r1End;		    /* End of 1st region	     */
    BoxPtr	    r2End;		    /* End of 2d region		     */
    int	            ybot;		    /* Bottom of intersection	     */
    int	            ytop;		    /* Top of intersection	     */
    RegDataPtr	    oldData;		    /* Old data for newReg	     */
    int		    prevBand;		    /* Index of start of
					     * previous band in newReg       */
    int		    curBand;		    /* Index of start of current
					     * band in newReg		     */
    register BoxPtr r1BandEnd;		    /* End of current band in r1     */
    register BoxPtr r2BandEnd;		    /* End of current band in r2     */
    int	            top;		    /* Top of non-overlapping band   */
    int	            bot;		    /* Bottom of non-overlapping band*/
    register int    r1y1;		    /* Temps for r1->y1 and r2->y1   */
    register int    r2y1;
    int		    newSize;
    int		    numRects;

    /*
     * Break any region computed from a broken region
     */
    if (REGION_NAR (reg1) || REGION_NAR(reg2))
	return miRegionBreak (newReg);
    
    /*
     * Initialization:
     *	set r1, r2, r1End and r2End appropriately, save the rectangles
     * of the destination region until the end in case it's one of
     * the two source regions, then mark the "new" region empty, allocating
     * another array of rectangles for it to use.
     */

    r1 = REGION_RECTS(reg1);
    newSize = REGION_NUM_RECTS(reg1);
    r1End = r1 + newSize;
    numRects = REGION_NUM_RECTS(reg2);
    r2 = REGION_RECTS(reg2);
    r2End = r2 + numRects;
    ASSERT(r1 != r1End);
    ASSERT(r2 != r2End);

    oldData = (RegDataPtr)NULL;
    if (((newReg == reg1) && (newSize > 1)) ||
	((newReg == reg2) && (numRects > 1)))
    {
	oldData = newReg->data;
	newReg->data = &miEmptyData;
    }
    /* guess at new size */
    if (numRects > newSize)
	newSize = numRects;
    newSize <<= 1;
    if (!newReg->data)
	newReg->data = &miEmptyData;
    else if (newReg->data->size)
	newReg->data->numRects = 0;
    if (newSize > newReg->data->size)
	if (!miRectAlloc(newReg, newSize))
	    return FALSE;

    /*
     * Initialize ybot.
     * In the upcoming loop, ybot and ytop serve different functions depending
     * on whether the band being handled is an overlapping or non-overlapping
     * band.
     * 	In the case of a non-overlapping band (only one of the regions
     * has points in the band), ybot is the bottom of the most recent
     * intersection and thus clips the top of the rectangles in that band.
     * ytop is the top of the next intersection between the two regions and
     * serves to clip the bottom of the rectangles in the current band.
     *	For an overlapping band (where the two regions intersect), ytop clips
     * the top of the rectangles of both regions and ybot clips the bottoms.
     */

    ybot = min(r1->y1, r2->y1);
    
    /*
     * prevBand serves to mark the start of the previous band so rectangles
     * can be coalesced into larger rectangles. qv. miCoalesce, above.
     * In the beginning, there is no previous band, so prevBand == curBand
     * (curBand is set later on, of course, but the first band will always
     * start at index 0). prevBand and curBand must be indices because of
     * the possible expansion, and resultant moving, of the new region's
     * array of rectangles.
     */
    prevBand = 0;
    
    do {
	/*
	 * This algorithm proceeds one source-band (as opposed to a
	 * destination band, which is determined by where the two regions
	 * intersect) at a time. r1BandEnd and r2BandEnd serve to mark the
	 * rectangle after the last one in the current band for their
	 * respective regions.
	 */
	ASSERT(r1 != r1End);
	ASSERT(r2 != r2End);
    
	FindBand(r1, r1BandEnd, r1End, r1y1);
	FindBand(r2, r2BandEnd, r2End, r2y1);

	/*
	 * First handle the band that doesn't intersect, if any.
	 *
	 * Note that attention is restricted to one band in the
	 * non-intersecting region at once, so if a region has n
	 * bands between the current position and the next place it overlaps
	 * the other, this entire loop will be passed through n times.
	 */
	if (r1y1 < r2y1) {
	    if (appendNon1) {
		top = max(r1y1, ybot);
		bot = min(r1->y2, r2y1);
		if (top != bot)	{
		    curBand = newReg->data->numRects;
		    miAppendNonO(newReg, r1, r1BandEnd, top, bot);
		    Coalesce(newReg, prevBand, curBand);
		}
	    }
	    ytop = r2y1;
	} else if (r2y1 < r1y1) {
	    if (appendNon2) {
		top = max(r2y1, ybot);
		bot = min(r2->y2, r1y1);
		if (top != bot) {
		    curBand = newReg->data->numRects;
		    miAppendNonO(newReg, r2, r2BandEnd, top, bot);
		    Coalesce(newReg, prevBand, curBand);
		}
	    }
	    ytop = r1y1;
	} else {
	    ytop = r1y1;
	}

	/*
	 * Now see if we've hit an intersecting band. The two bands only
	 * intersect if ybot > ytop
	 */
	ybot = min(r1->y2, r2->y2);
	if (ybot > ytop) {
	    curBand = newReg->data->numRects;
	    (* overlapFunc)(newReg, r1, r1BandEnd, r2, r2BandEnd, ytop, ybot,
			    pOverlap);
	    Coalesce(newReg, prevBand, curBand);
	}

	/*
	 * If we've finished with a band (y2 == ybot) we skip forward
	 * in the region to the next band.
	 */
	if (r1->y2 == ybot) r1 = r1BandEnd;
	if (r2->y2 == ybot) r2 = r2BandEnd;

    } while (r1 != r1End && r2 != r2End);

    /*
     * Deal with whichever region (if any) still has rectangles left.
     *
     * We only need to worry about banding and coalescing for the very first
     * band left.  After that, we can just group all remaining boxes,
     * regardless of how many bands, into one final append to the list.
     */

    if ((r1 != r1End) && appendNon1) {
	/* Do first nonOverlap1Func call, which may be able to coalesce */
	FindBand(r1, r1BandEnd, r1End, r1y1);
	curBand = newReg->data->numRects;
	miAppendNonO(newReg, r1, r1BandEnd, max(r1y1, ybot), r1->y2);
	Coalesce(newReg, prevBand, curBand);
	/* Just append the rest of the boxes  */
	AppendRegions(newReg, r1BandEnd, r1End);

    } else if ((r2 != r2End) && appendNon2) {
	/* Do first nonOverlap2Func call, which may be able to coalesce */
	FindBand(r2, r2BandEnd, r2End, r2y1);
	curBand = newReg->data->numRects;
	miAppendNonO(newReg, r2, r2BandEnd, max(r2y1, ybot), r2->y2);
	Coalesce(newReg, prevBand, curBand);
	/* Append rest of boxes */
	AppendRegions(newReg, r2BandEnd, r2End);
    }

    if (oldData)
	xfree(oldData);

    if (!(numRects = newReg->data->numRects))
    {
	xfreeData(newReg);
	newReg->data = &miEmptyData;
    }
    else if (numRects == 1)
    {
	newReg->extents = *REGION_BOXPTR(newReg);
	xfreeData(newReg);
	newReg->data = (RegDataPtr)NULL;
    }
    else
    {
	DOWNSIZE(newReg, numRects);
    }

    return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * miSetExtents --
 *	Reset the extents of a region to what they should be. Called by
 *	miSubtract and miIntersect as they can't figure it out along the
 *	way or do so easily, as miUnion can.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The region's 'extents' structure is overwritten.
 *
 *-----------------------------------------------------------------------
 */
void
miSetExtents (pReg)
    register RegionPtr pReg;
{
    register BoxPtr pBox, pBoxEnd;

    if (!pReg->data)
	return;
    if (!pReg->data->size)
    {
	pReg->extents.x2 = pReg->extents.x1;
	pReg->extents.y2 = pReg->extents.y1;
	return;
    }

    pBox = REGION_BOXPTR(pReg);
    pBoxEnd = REGION_END(pReg);

    /*
     * Since pBox is the first rectangle in the region, it must have the
     * smallest y1 and since pBoxEnd is the last rectangle in the region,
     * it must have the largest y2, because of banding. Initialize x1 and
     * x2 from  pBox and pBoxEnd, resp., as good things to initialize them
     * to...
     */
    pReg->extents.x1 = pBox->x1;
    pReg->extents.y1 = pBox->y1;
    pReg->extents.x2 = pBoxEnd->x2;
    pReg->extents.y2 = pBoxEnd->y2;

    ASSERT(pReg->extents.y1 < pReg->extents.y2);
    while (pBox <= pBoxEnd) {
	if (pBox->x1 < pReg->extents.x1)
	    pReg->extents.x1 = pBox->x1;
	if (pBox->x2 > pReg->extents.x2)
	    pReg->extents.x2 = pBox->x2;
	pBox++;
    };

    ASSERT(pReg->extents.x1 < pReg->extents.x2);
}

/*======================================================================
 *	    Region Intersection
 *====================================================================*/
/*-
 *-----------------------------------------------------------------------
 * miIntersectO --
 *	Handle an overlapping band for miIntersect.
 *
 * Results:
 *	TRUE if successful.
 *
 * Side Effects:
 *	Rectangles may be added to the region.
 *
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/
static Bool
miIntersectO (
    register RegionPtr	pReg,
    register BoxPtr	r1,
    BoxPtr  	  	r1End,
    register BoxPtr	r2,
    BoxPtr  	  	r2End,
    int    	  	y1,
    int    	  	y2,
    Bool		*pOverlap)
{
    register int  	x1;
    register int  	x2;
    register BoxPtr	pNextRect;

    pNextRect = REGION_TOP(pReg);

    ASSERT(y1 < y2);
    ASSERT(r1 != r1End && r2 != r2End);

    do {
	x1 = max(r1->x1, r2->x1);
	x2 = min(r1->x2, r2->x2);

	/*
         * If there's any overlap between the two rectangles, add that overlap
         * to the new region, using r2's RectInfo for the new rectangle.
	 */
	if (x1 < x2) {
            RectInfo info = r2->info;

            miRectInfoAdjustForSplit(&info, r2->x1, r2->y1, x1, y1);
	    NEWRECT(pReg, pNextRect, x1, y1, x2, y2, info);
        }

	/*
	 * Advance the pointer(s) with the leftmost right side, since the next
	 * rectangle on that list may still overlap the other region's
	 * current rectangle.
	 */
	if (r1->x2 == x2) {
	    r1++;
	}
	if (r2->x2 == x2) {
	    r2++;
	}
    } while ((r1 != r1End) && (r2 != r2End));

    return TRUE;
}


Bool
miIntersect(newReg, reg1, reg2)
    register RegionPtr 	newReg;     /* destination Region */
    register RegionPtr 	reg1;
    register RegionPtr	reg2;       /* source regions     */
{
    good(reg1);
    good(reg2);
    good(newReg);
   /* check for trivial reject */
    if (REGION_NIL(reg1)  || REGION_NIL(reg2) ||
	!EXTENTCHECK(&reg1->extents, &reg2->extents))
    {
	/* Covers about 20% of all cases */
	xfreeData(newReg);
	newReg->extents.x2 = newReg->extents.x1;
	newReg->extents.y2 = newReg->extents.y1;
	if (REGION_NAR(reg1) || REGION_NAR(reg2))
	{
	    newReg->data = &miBrokenData;
	    return FALSE;
	}
	else
	    newReg->data = &miEmptyData;
    }
    else if (!reg1->data && !reg2->data)
    {
	/* Covers about 80% of cases that aren't trivially rejected */
	newReg->extents.x1 = max(reg1->extents.x1, reg2->extents.x1);
	newReg->extents.y1 = max(reg1->extents.y1, reg2->extents.y1);
	newReg->extents.x2 = min(reg1->extents.x2, reg2->extents.x2);
	newReg->extents.y2 = min(reg1->extents.y2, reg2->extents.y2);
        newReg->extents.info = reg2->extents.info;
        miRectInfoAdjustForSplit(&newReg->extents.info,
                                 reg2->extents.x1, reg2->extents.y1,
                                 newReg->extents.x1, newReg->extents.y1);
	xfreeData(newReg);
	newReg->data = (RegDataPtr)NULL;
    }
    else if (!reg2->data && SUBSUMES(&reg2->extents, &reg1->extents))
    {
       int i;
       Bool res;

       /*
        * Result is reg1 with reg2's info copied into all rectangles.
        */
       if ((res = miRegionCopy(newReg, reg1)))
       {
          newReg->extents.info = reg2->extents.info;
          miRectInfoAdjustForSplit(&newReg->extents.info,
                                   reg2->extents.x1, reg2->extents.y1,
                                   newReg->extents.x1, newReg->extents.y1);
          for (i = 0; i < newReg->data->numRects; i++)
          {
             BoxPtr box = REGION_BOX(newReg, i);
             box->info = reg2->extents.info;
             miRectInfoAdjustForSplit(&box->info,
                                      reg2->extents.x1, reg2->extents.y1,
                                      box->x1, box->y1);
          }
       }
       return res;
    }
    else if (!reg1->data && SUBSUMES(&reg1->extents, &reg2->extents))
    {
	return miRegionCopy(newReg, reg2);
    }
    else if (reg1 == reg2)
    {
	return miRegionCopy(newReg, reg1);
    }
    else
    {
	/* General purpose intersection */
	Bool overlap; /* result ignored */
	if (!miRegionOp(newReg, reg1, reg2, miIntersectO, FALSE, FALSE,
			&overlap))
	    return FALSE;
	miSetExtents(newReg);
    }

    good(newReg);
    return(TRUE);
}

#define MERGERECT(r)						\
{								\
    if (r->x1 <= x2) {						\
	/* Merge with current rectangle */			\
	if (r->x1 < x2) *pOverlap = TRUE;				\
	if (x2 < r->x2) x2 = r->x2;				\
    } else {							\
	/* Add current rectangle, start new one */		\
	NEWRECT(pReg, pNextRect, x1, y1, x2, y2);		\
	x1 = r->x1;						\
	x2 = r->x2;						\
    }								\
    r++;							\
}

/*======================================================================
 *	    Region Union
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miUnionO --
 *	Handle an overlapping band for the union operation. Picks the
 *	left-most rectangle each time and merges it into the region.
 *
 * Results:
 *	TRUE if successful.
 *
 * Side Effects:
 *	pReg is overwritten.
 *	pOverlap is set to TRUE if any boxes overlap.
 *
 *-----------------------------------------------------------------------
 */
static Bool
miUnionO (
    register RegionPtr	pReg,
    register BoxPtr	r1,
	     BoxPtr  	r1End,
    register BoxPtr	r2,
	     BoxPtr  	r2End,
	     int	y1,
	     int	y2,
	     Bool	*pOverlap)
{
    register BoxPtr     pNextRect;

    RegionRec subReg;
    RegionRec tmpReg;
    BoxPtr pNextRectTmp;
    int x1, x2;
    RectInfo info;

    ASSERT (y1 < y2);
    ASSERT(r1 != r1End && r2 != r2End);

    miRegionInit(&subReg, NULL, r1End - r1);
    miRegionInit(&tmpReg, NULL, r1End - r1);

    pNextRectTmp = REGION_BOXPTR((&tmpReg));

    /*
     * XXX We compute the union as (reg 1 - reg 2) + (reg 2). Is there a
     *     better way?
     */
    miSubtractO(&subReg, r1, r1End, r2, r2End, y1, y2, pOverlap);

    r1 = REGION_BOXPTR(&subReg);
    r1End = REGION_TOP(&subReg);

    /*
     * Add the new rectangles in the correct order into a temporary list
     */
    while (r1 != r1End || r2 != r2End) {
       if (r1 != r1End && (r2 == r2End || (r1->x1 < r2->x1))) {
          info = r1->info;
          if (y1 > r1->y1) {
             miRectInfoAdjustForSplit(&info, 
                                      r1->x1, r1->y1,
                                      r1->x1, y1);
          }
          NEWRECT((&tmpReg), pNextRectTmp, r1->x1, y1, r1->x2, y2, info);
          r1++;
       } else {
          info = r2->info;
          if (y1 > r2->y1) {
             miRectInfoAdjustForSplit(&info, 
                                      r2->x1, r2->y1,
                                      r2->x1, y1);
          }
          NEWRECT((&tmpReg), pNextRectTmp, r2->x1, y1, r2->x2, y2, info);
          r2++;
       }
    }

    /*
     * Copy the rectangles into the destination region, merging where possible
     */
    for (pNextRectTmp = REGION_BOXPTR(&tmpReg),
         x1 = pNextRectTmp->x1, x2 = pNextRectTmp->x2, info = pNextRectTmp->info,
         pNextRect = REGION_TOP(pReg);
         pNextRectTmp != REGION_TOP(&tmpReg);
         pNextRectTmp++) {

       if (pNextRectTmp->x1 > x2 ||
           !miRectInfosHorzAdjacent(&pNextRectTmp->info, pNextRectTmp->x1,
                                    &info, x1)) {
          NEWRECT(pReg, pNextRect, x1, y1, x2, y2, info);
          x1 = pNextRectTmp->x1;
          x2 = pNextRectTmp->x2;
          info = pNextRectTmp->info;
       } else {
          x2 = pNextRectTmp->x2;
       }
    }

    NEWRECT(pReg, pNextRect, x1, y1, x2, y2, info);

    xfreeData(&subReg);
    xfreeData(&tmpReg);

    return TRUE;
}

Bool 
miUnion(newReg, reg1, reg2)
    RegionPtr		newReg;                  /* destination Region */
    register RegionPtr 	reg1;
    register RegionPtr	reg2;             /* source regions     */
{
    Bool overlap; /* result ignored */

    /* Return TRUE if some overlap between reg1, reg2 */
    good(reg1);
    good(reg2);
    good(newReg);
    /*  checks all the simple cases */

    /*
     * Region 1 and 2 are the same
     */
    if (reg1 == reg2)
    {
	return miRegionCopy(newReg, reg1);
    }

    /*
     * Region 1 is empty
     */
    if (miIsRegionVoid(reg1))
    {
	if (REGION_NAR(reg1))
	    return miRegionBreak (newReg);
        if (newReg != reg2)
	    return miRegionCopy(newReg, reg2);
        return TRUE;
    }

    /*
     * Region 2 is empty
     */
    if (miIsRegionVoid(reg2)) 
    {
	if (REGION_NAR(reg2))
	    return miRegionBreak (newReg);
        if (newReg != reg1)
	    return miRegionCopy(newReg, reg1);
        return TRUE;
    }

    /*
     * Region 1 completely subsumes region 2
     *
     * We still need to do the complete union calculation, because reg 2
     * may have accelerated operations that need to be stencilled onto reg 1
     */

    /*
    if (!reg1->data && SUBSUMES(&reg1->extents, &reg2->extents))
    {
        if (newReg != reg1)
          return miRegionCopy(newReg, reg1);
        return TRUE;
    }
    */

    /*
     * Region 2 completely subsumes region 1
     */
    if (!reg2->data && SUBSUMES(&reg2->extents, &reg1->extents))
    {
        if (newReg != reg2)
	    return miRegionCopy(newReg, reg2);
        return TRUE;
    }

    if (!miRegionOp(newReg, reg1, reg2, miUnionO, TRUE, TRUE, &overlap))
	return FALSE;

    /*
     * This previously failed when we called REGION_NIL on reg1 and reg2;
     * a region could be !REGION_NIL but have an extent, say 0, 0, 0, 0,
     * which would then overwrite newReg with an invalid extent.
     *
     * Using miIsRegionVoid() instead of testing REGION_NIL fixes that
     * bug.
     */
    newReg->extents.x1 = min(reg1->extents.x1, reg2->extents.x1);
    newReg->extents.y1 = min(reg1->extents.y1, reg2->extents.y1);
    newReg->extents.x2 = max(reg1->extents.x2, reg2->extents.x2);
    newReg->extents.y2 = max(reg1->extents.y2, reg2->extents.y2);
    good(newReg);
    return TRUE;
}


/*======================================================================
 *	    Batch Rectangle Union
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miRegionAppend --
 * 
 *      "Append" the rgn rectangles onto the end of dstrgn, maintaining
 *      knowledge of YX-banding when it's easy.  Otherwise, dstrgn just
 *      becomes a non-y-x-banded random collection of rectangles, and not
 *      yet a true region.  After a sequence of appends, the caller must
 *      call miRegionValidate to ensure that a valid region is constructed.
 *
 * Results:
 *	TRUE if successful.
 *
 * Side Effects:
 *      dstrgn is modified if rgn has rectangles.
 *
 */
Bool
miRegionAppend(dstrgn, rgn)
    register RegionPtr dstrgn;
    register RegionPtr rgn;
{
    int numRects, dnumRects, size;
    BoxPtr new, old;
    Bool prepend;

    if (REGION_NAR(rgn))
	return miRegionBreak (dstrgn);
    
    if (!rgn->data && (dstrgn->data == &miEmptyData))
    {
	dstrgn->extents = rgn->extents;
	dstrgn->data = (RegDataPtr)NULL;
	return TRUE;
    }

    numRects = REGION_NUM_RECTS(rgn);
    if (!numRects)
	return TRUE;
    prepend = FALSE;
    size = numRects;
    dnumRects = REGION_NUM_RECTS(dstrgn);
    if (!dnumRects && (size < 200))
	size = 200; /* XXX pick numbers out of a hat */
    RECTALLOC(dstrgn, size);
    old = REGION_RECTS(rgn);
    if (!dnumRects)
	dstrgn->extents = rgn->extents;
    else if (dstrgn->extents.x2 > dstrgn->extents.x1)
    {
	register BoxPtr first, last;

	first = old;
	last = REGION_BOXPTR(dstrgn) + (dnumRects - 1);
	if ((first->y1 > last->y2) ||
	    ((first->y1 == last->y1) && (first->y2 == last->y2) &&
	     (first->x1 > last->x2)))
	{
	    if (rgn->extents.x1 < dstrgn->extents.x1)
		dstrgn->extents.x1 = rgn->extents.x1;
	    if (rgn->extents.x2 > dstrgn->extents.x2)
		dstrgn->extents.x2 = rgn->extents.x2;
	    dstrgn->extents.y2 = rgn->extents.y2;
	}
	else
	{
	    first = REGION_BOXPTR(dstrgn);
	    last = old + (numRects - 1);
	    if ((first->y1 > last->y2) ||
		((first->y1 == last->y1) && (first->y2 == last->y2) &&
		 (first->x1 > last->x2)))
	    {
		prepend = TRUE;
		if (rgn->extents.x1 < dstrgn->extents.x1)
		    dstrgn->extents.x1 = rgn->extents.x1;
		if (rgn->extents.x2 > dstrgn->extents.x2)
		    dstrgn->extents.x2 = rgn->extents.x2;
		dstrgn->extents.y1 = rgn->extents.y1;
	    }
	    else
		dstrgn->extents.x2 = dstrgn->extents.x1;
	}
    }
    if (prepend)
    {
	new = REGION_BOX(dstrgn, numRects);
	if (dnumRects == 1)
	    *new = *REGION_BOXPTR(dstrgn);
	else
	    memmove((char *)new,(char *)REGION_BOXPTR(dstrgn), 
		  dnumRects * sizeof(BoxRec));
	new = REGION_BOXPTR(dstrgn);
    }
    else
	new = REGION_BOXPTR(dstrgn) + dnumRects;
    if (numRects == 1)
	*new = *old;
    else
	memmove((char *)new, (char *)old, numRects * sizeof(BoxRec));
    dstrgn->data->numRects += numRects;
    return TRUE;
}

   
#define ExchangeRects(a, b) \
{			    \
    BoxRec     t;	    \
    t = rects[a];	    \
    rects[a] = rects[b];    \
    rects[b] = t;	    \
}

static void
QuickSortRects(
    register BoxRec     rects[],
    register int        numRects)
{
    register int	y1;
    register int	x1;
    register int        i, j;
    register BoxPtr     r;

    /* Always called with numRects > 1 */

    do
    {
	if (numRects == 2)
	{
	    if (rects[0].y1 > rects[1].y1 ||
		    (rects[0].y1 == rects[1].y1 && rects[0].x1 > rects[1].x1))
		ExchangeRects(0, 1);
	    return;
	}

	/* Choose partition element, stick in location 0 */
        ExchangeRects(0, numRects >> 1);
	y1 = rects[0].y1;
	x1 = rects[0].x1;

        /* Partition array */
        i = 0;
        j = numRects;
        do
	{
	    r = &(rects[i]);
	    do
	    {
		r++;
		i++;
            } while (i != numRects &&
		     (r->y1 < y1 || (r->y1 == y1 && r->x1 < x1)));
	    r = &(rects[j]);
	    do
	    {
		r--;
		j--;
            } while (y1 < r->y1 || (y1 == r->y1 && x1 < r->x1));
            if (i < j)
		ExchangeRects(i, j);
        } while (i < j);

        /* Move partition element back to middle */
        ExchangeRects(0, j);

	/* Recurse */
        if (numRects-j-1 > 1)
	    QuickSortRects(&rects[j+1], numRects-j-1);
        numRects = j;
    } while (numRects > 1);
}

/*-
 *-----------------------------------------------------------------------
 * miRegionValidate --
 * 
 *      Take a ``region'' which is a non-y-x-banded random collection of
 *      rectangles, and compute a nice region which is the union of all the
 *      rectangles.
 *
 * Results:
 *	TRUE if successful.
 *
 * Side Effects:
 *      The passed-in ``region'' may be modified.
 *	pOverlap set to TRUE if any retangles overlapped, else FALSE;
 *
 * Strategy:
 *      Step 1. Sort the rectangles into ascending order with primary key y1
 *		and secondary key x1.
 *
 *      Step 2. Split the rectangles into the minimum number of proper y-x
 *		banded regions.  This may require horizontally merging
 *		rectangles, and vertically coalescing bands.  With any luck,
 *		this step in an identity tranformation (ala the Box widget),
 *		or a coalescing into 1 box (ala Menus).
 *
 *	Step 3. Merge the separate regions down to a single region by calling
 *		miUnion.  Maximize the work each miUnion call does by using
 *		a binary merge.
 *
 *-----------------------------------------------------------------------
 */

Bool
miRegionValidate(badreg, pOverlap)
    RegionPtr badreg;
    Bool *pOverlap;
{
    /* Descriptor for regions under construction  in Step 2. */
    typedef struct {
	RegionRec   reg;
	int	    prevBand;
	int	    curBand;
    } RegionInfo;

	     int	numRects;   /* Original numRects for badreg	    */
	     RegionInfo *ri;	    /* Array of current regions		    */
    	     int	numRI;      /* Number of entries used in ri	    */
	     int	sizeRI;	    /* Number of entries available in ri    */
	     int	i;	    /* Index into rects			    */
    register int	j;	    /* Index into ri			    */
    register RegionInfo *rit;       /* &ri[j]				    */
    register RegionPtr  reg;        /* ri[j].reg			    */
    register BoxPtr	box;	    /* Current box in rects		    */
    register BoxPtr	riBox;      /* Last box in ri[j].reg		    */
    register RegionPtr  hreg;       /* ri[j_half].reg			    */
    Bool		ret = TRUE;

    *pOverlap = FALSE;
    if (!badreg->data)
    {
	good(badreg);
	return TRUE;
    }
    numRects = badreg->data->numRects;
    if (!numRects)
    {
	if (REGION_NAR(badreg))
	    return FALSE;
	good(badreg);
	return TRUE;
    }
    if (badreg->extents.x1 < badreg->extents.x2)
    {
	if ((numRects) == 1)
	{
	    xfreeData(badreg);
	    badreg->data = (RegDataPtr) NULL;
	}
	else
	{
	    DOWNSIZE(badreg, numRects);
	}
	good(badreg);
	return TRUE;
    }

    /* Step 1: Sort the rects array into ascending (y1, x1) order */
    QuickSortRects(REGION_BOXPTR(badreg), numRects);

    /* Step 2: Scatter the sorted array into the minimum number of regions */

    /* Set up the first region to be the first rectangle in badreg */
    /* Note that step 2 code will never overflow the ri[0].reg rects array */
    ri = (RegionInfo *) xalloc(4 * sizeof(RegionInfo));
    if (!ri)
	return miRegionBreak (badreg);
    sizeRI = 4;
    numRI = 1;
    ri[0].prevBand = 0;
    ri[0].curBand = 0;
    ri[0].reg = *badreg;
    box = REGION_BOXPTR(&ri[0].reg);
    ri[0].reg.extents = *box;
    ri[0].reg.data->numRects = 1;

    /* Now scatter rectangles into the minimum set of valid regions.  If the
       next rectangle to be added to a region would force an existing rectangle
       in the region to be split up in order to maintain y-x banding, just
       forget it.  Try the next region.  If it doesn't fit cleanly into any
       region, make a new one. */

    for (i = numRects; --i > 0;)
    {
	box++;
	/* Look for a region to append box to */
	for (j = numRI, rit = ri; --j >= 0; rit++)
	{
	    reg = &rit->reg;
	    riBox = REGION_END(reg);

	    if (box->y1 == riBox->y1 && box->y2 == riBox->y2)
	    {
		/* box is in same band as riBox.  Merge or append it */
		if (box->x1 <= riBox->x2)
		{
		    /* Merge it with riBox */
		    if (box->x1 < riBox->x2) *pOverlap = TRUE;
		    if (box->x2 > riBox->x2) riBox->x2 = box->x2;
		}
		else
		{
		    RECTALLOC_BAIL(reg, 1, bail);
		    *REGION_TOP(reg) = *box;
		    reg->data->numRects++;
		}
		goto NextRect;   /* So sue me */
	    }
	    else if (box->y1 >= riBox->y2)
	    {
		/* Put box into new band */
		if (reg->extents.x2 < riBox->x2) reg->extents.x2 = riBox->x2;
		if (reg->extents.x1 > box->x1)   reg->extents.x1 = box->x1;
		Coalesce(reg, rit->prevBand, rit->curBand);
		rit->curBand = reg->data->numRects;
		RECTALLOC_BAIL(reg, 1, bail);
		*REGION_TOP(reg) = *box;
		reg->data->numRects++;
		goto NextRect;
	    }
	    /* Well, this region was inappropriate.  Try the next one. */
	} /* for j */

	/* Uh-oh.  No regions were appropriate.  Create a new one. */
	if (sizeRI == numRI)
	{
	    /* Oops, allocate space for new region information */
	    sizeRI <<= 1;
	    rit = (RegionInfo *) xrealloc(ri, sizeRI * sizeof(RegionInfo));
	    if (!rit)
		goto bail;
	    ri = rit;
	    rit = &ri[numRI];
	}
	numRI++;
	rit->prevBand = 0;
	rit->curBand = 0;
	rit->reg.extents = *box;
	rit->reg.data = (RegDataPtr)NULL;
	if (!miRectAlloc(&rit->reg, (i+numRI) / numRI)) /* MUST force allocation */
	    goto bail;
NextRect: ;
    } /* for i */

    /* Make a final pass over each region in order to Coalesce and set
       extents.x2 and extents.y2 */

    for (j = numRI, rit = ri; --j >= 0; rit++)
    {
	reg = &rit->reg;
	riBox = REGION_END(reg);
	reg->extents.y2 = riBox->y2;
	if (reg->extents.x2 < riBox->x2) reg->extents.x2 = riBox->x2;
	Coalesce(reg, rit->prevBand, rit->curBand);
	if (reg->data->numRects == 1) /* keep unions happy below */
	{
	    xfreeData(reg);
	    reg->data = (RegDataPtr)NULL;
	}
    }

    /* Step 3: Union all regions into a single region */
    while (numRI > 1)
    {
	int half = numRI/2;
	for (j = numRI & 1; j < (half + (numRI & 1)); j++)
	{
	    reg = &ri[j].reg;
	    hreg = &ri[j+half].reg;
	    if (!miRegionOp(reg, reg, hreg, miUnionO, TRUE, TRUE, pOverlap))
		ret = FALSE;
	    if (hreg->extents.x1 < reg->extents.x1)
		reg->extents.x1 = hreg->extents.x1;
	    if (hreg->extents.y1 < reg->extents.y1)
		reg->extents.y1 = hreg->extents.y1;
	    if (hreg->extents.x2 > reg->extents.x2)
		reg->extents.x2 = hreg->extents.x2;
	    if (hreg->extents.y2 > reg->extents.y2)
		reg->extents.y2 = hreg->extents.y2;
	    xfreeData(hreg);
	}
	numRI -= half;
    }
    *badreg = ri[0].reg;
    xfree(ri);
    good(badreg);
    return ret;
bail:
    for (i = 0; i < numRI; i++)
	xfreeData(&ri[i].reg);
    xfree (ri);
    return miRegionBreak (badreg);
}

RegionPtr
miRectsToRegionByBoundary(
                          int			nrects,
                          register xRectangle	*prect,
                          int			ctype,
                          int                 minValue,
                          int                 maxValue)
{
    register RegionPtr	pRgn;
    register RegDataPtr	pData;
    register BoxPtr	pBox;
    register int        i;
    int			x1, y1, x2, y2;

    pRgn = miRegionCreate(NullBox, 0);
    if (REGION_NAR (pRgn))
	return pRgn;
    if (!nrects)
	return pRgn;
    if (nrects == 1)
    {
        x1 = prect->x < minValue ? minValue : prect->x;
        y1 = prect->y < minValue ? minValue : prect->y;
	if ((x2 = x1 + (int) prect->width) > maxValue)
	    x2 = maxValue;
	if ((y2 = y1 + (int) prect->height) > maxValue)
	    y2 = maxValue;
	if (x1 != x2 && y1 != y2)
	{
	    pRgn->extents.x1 = x1;
	    pRgn->extents.y1 = y1;
	    pRgn->extents.x2 = x2;
	    pRgn->extents.y2 = y2;
	    pRgn->data = (RegDataPtr)NULL;
	}
	return pRgn;
    }
    pData = xallocData(nrects);
    if (!pData)
    {
	miRegionBreak (pRgn);
	return pRgn;
    }
    pBox = (BoxPtr) (pData + 1);
    for (i = nrects; --i >= 0; prect++)
    {
        x1 = prect->x < minValue ? minValue : prect->x;
        y1 = prect->y < minValue ? minValue : prect->y;
	if ((x2 = x1 + (int) prect->width) > maxValue)
	    x2 = maxValue;
	if ((y2 = y1 + (int) prect->height) > maxValue)
	    y2 = maxValue;
	if (x1 != x2 && y1 != y2)
	{
	    pBox->x1 = x1;
	    pBox->y1 = y1;
	    pBox->x2 = x2;
	    pBox->y2 = y2;
	    pBox->info.type = UpdateRect;
	    pBox++;
	}
    }
    if (pBox != (BoxPtr) (pData + 1))
    {
	pData->size = nrects;
	pData->numRects = pBox - (BoxPtr) (pData + 1);
    	pRgn->data = pData;
    	if (ctype != CT_YXBANDED)
    	{
	    Bool overlap; /* result ignored */
	    pRgn->extents.x1 = pRgn->extents.x2 = 0;
	    miRegionValidate(pRgn, &overlap);
    	}
    	else
	    miSetExtents(pRgn);
    	good(pRgn);
    }
    else
    {
	xfree (pData);
    }
    return pRgn;
}

/*
 * Construct the region from rects using the default values
 * R_MININT and R_MAXINT for boundary check
 */
RegionPtr
miRectsToRegion(
                int			nrects,
                register xRectangle	*prect,
                int			ctype)
{
   return miRectsToRegionByBoundary(nrects, prect, ctype,
                                    R_MININT, R_MAXINT);
}

/*======================================================================
 * 	    	  Region Subtraction
 *====================================================================*/


/*-
 *-----------------------------------------------------------------------
 * miSubtractO --
 *	Overlapping band subtraction. x1 is the left-most point not yet
 *	checked.
 *
 * Results:
 *	TRUE if successful.
 *
 * Side Effects:
 *	pReg may have rectangles added to it.
 *
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/
static Bool
miSubtractO (
    register RegionPtr	pReg,
    register BoxPtr	r1,
    BoxPtr  	  	r1End,
    register BoxPtr	r2,
    BoxPtr  	  	r2End,
    register int  	y1,
             int  	y2,
    Bool		*pOverlap)
{
    register BoxPtr	pNextRect;
    register int  	x1;

    x1 = r1->x1;
    
    ASSERT(y1<y2);
    ASSERT(r1 != r1End && r2 != r2End);

    pNextRect = REGION_TOP(pReg);

    do
    {
	if (r2->x2 <= x1)
	{
	    /*
	     * Subtrahend entirely to left of minuend: go to next subtrahend.
	     */
	    r2++;
	}
	else if (r2->x1 <= x1)
	{
	    /*
	     * Subtrahend preceeds minuend: nuke left edge of minuend.
	     */
	    x1 = r2->x2;
	    if (x1 >= r1->x2)
	    {
		/*
		 * Minuend completely covered: advance to next minuend and
		 * reset left fence to edge of new minuend.
		 */
		r1++;
		if (r1 != r1End)
		    x1 = r1->x1;
	    }
	    else
	    {
		/*
		 * Subtrahend now used up since it doesn't extend beyond
		 * minuend
		 */
		r2++;
	    }
	}
	else if (r2->x1 < r1->x2)
	{
	    /*
	     * Left part of subtrahend covers part of minuend: add uncovered
	     * part of minuend to region and skip to next subtrahend.
	     */
            RectInfo info = r1->info;
            ASSERT(x1 < r2->x1);
            miRectInfoAdjustForSplit(&info, r1->x1, r1->y1, x1, y1);
            NEWRECT(pReg, pNextRect, x1, y1, r2->x1, y2, info);

	    x1 = r2->x2;
	    if (x1 >= r1->x2)
	    {
		/*
		 * Minuend used up: advance to new...
		 */
		r1++;
		if (r1 != r1End)
		    x1 = r1->x1;
	    }
	    else
	    {
		/*
		 * Subtrahend used up
		 */
		r2++;
	    }
	}
	else
	{
	    /*
	     * Minuend used up: add any remaining piece before advancing.
	     */
            if (r1->x2 > x1) {
                RectInfo info = r1->info;

                miRectInfoAdjustForSplit(&info, r1->x1, r1->y1, x1, y1);
		NEWRECT(pReg, pNextRect, x1, y1, r1->x2, y2, info);
            }
	    r1++;
	    if (r1 != r1End)
		x1 = r1->x1;
	}
    } while ((r1 != r1End) && (r2 != r2End));


    /*
     * Add remaining minuend rectangles to region.
     */
    while (r1 != r1End)
    {
        RectInfo info = r1->info;

	ASSERT(x1<r1->x2);
        miRectInfoAdjustForSplit(&info, r1->x1, r1->y1, x1, y1);
	NEWRECT(pReg, pNextRect, x1, y1, r1->x2, y2, info);
	r1++;
	if (r1 != r1End)
	    x1 = r1->x1;
    }
    return TRUE;
}
	
/*-
 *-----------------------------------------------------------------------
 * miSubtract --
 *	Subtract regS from regM and leave the result in regD.
 *	S stands for subtrahend, M for minuend and D for difference.
 *
 * Results:
 *	TRUE if successful.
 *
 * Side Effects:
 *	regD is overwritten.
 *
 *-----------------------------------------------------------------------
 */
Bool
miSubtract(regD, regM, regS)
    register RegionPtr	regD;               
    register RegionPtr 	regM;
    register RegionPtr	regS;          
{
    Bool overlap; /* result ignored */

    good(regM);
    good(regS);
    good(regD);
   /* check for trivial rejects */
    if (REGION_NIL(regM) || REGION_NIL(regS) ||
	!EXTENTCHECK(&regM->extents, &regS->extents))
    {
	if (REGION_NAR (regS))
	    return miRegionBreak (regD);
	return miRegionCopy(regD, regM);
    }
    else if (regM == regS)
    {
	xfreeData(regD);
	regD->extents.x2 = regD->extents.x1;
	regD->extents.y2 = regD->extents.y1;
	regD->data = &miEmptyData;
	return TRUE;
    }
 
    /* Add those rectangles in region 1 that aren't in region 2,
       do yucky subtraction for overlaps, and
       just throw away rectangles in region 2 that aren't in region 1 */
    if (!miRegionOp(regD, regM, regS, miSubtractO, TRUE, FALSE, &overlap))
	return FALSE;

    /*
     * Can't alter RegD's extents before we call miRegionOp because
     * it might be one of the source regions and miRegionOp depends
     * on the extents of those regions being unaltered. Besides, this
     * way there's no checking against rectangles that will be nuked
     * due to coalescing, so we have to examine fewer rectangles.
     */
    miSetExtents(regD);
    good(regD);
    return TRUE;
}

/*======================================================================
 *	    Region Inversion
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miInverse --
 *	Take a region and a box and return a region that is everything
 *	in the box but not in the region. The careful reader will note
 *	that this is the same as subtracting the region from the box...
 *
 * Results:
 *	TRUE.
 *
 * Side Effects:
 *	newReg is overwritten.
 *
 *-----------------------------------------------------------------------
 */
Bool
miInverse(newReg, reg1, invRect)
    RegionPtr 	  newReg;       /* Destination region */
    RegionPtr 	  reg1;         /* Region to invert */
    BoxPtr     	  invRect; 	/* Bounding box for inversion */
{
    RegionRec	  invReg;   	/* Quick and dirty region made from the
				 * bounding box */
    Bool	  overlap;	/* result ignored */

    good(reg1);
    good(newReg);
   /* check for trivial rejects */
    if (REGION_NIL(reg1) || !EXTENTCHECK(invRect, &reg1->extents))
    {
	if (REGION_NAR(reg1))
	    return miRegionBreak (newReg);
	newReg->extents = *invRect;
	xfreeData(newReg);
	newReg->data = (RegDataPtr)NULL;
        return TRUE;
    }

    /* Add those rectangles in region 1 that aren't in region 2,
       do yucky subtraction for overlaps, and
       just throw away rectangles in region 2 that aren't in region 1 */
    invReg.extents = *invRect;
    invReg.data = (RegDataPtr)NULL;
    if (!miRegionOp(newReg, &invReg, reg1, miSubtractO, TRUE, FALSE, &overlap))
	return FALSE;

    /*
     * Can't alter newReg's extents before we call miRegionOp because
     * it might be one of the source regions and miRegionOp depends
     * on the extents of those regions being unaltered. Besides, this
     * way there's no checking against rectangles that will be nuked
     * due to coalescing, so we have to examine fewer rectangles.
     */
    miSetExtents(newReg);
    good(newReg);
    return TRUE;
}

/*
 *   RectIn(region, rect)
 *   This routine takes a pointer to a region and a pointer to a box
 *   and determines if the box is outside/inside/partly inside the region.
 *
 *   The idea is to travel through the list of rectangles trying to cover the
 *   passed box with them. Anytime a piece of the rectangle isn't covered
 *   by a band of rectangles, partOut is set TRUE. Any time a rectangle in
 *   the region covers part of the box, partIn is set TRUE. The process ends
 *   when either the box has been completely covered (we reached a band that
 *   doesn't overlap the box, partIn is TRUE and partOut is false), the
 *   box has been partially covered (partIn == partOut == TRUE -- because of
 *   the banding, the first time this is true we know the box is only
 *   partially in the region) or is outside the region (we reached a band
 *   that doesn't overlap the box at all and partIn is false)
 */

int
miRectIn(region, prect)
    register RegionPtr  region;
    register BoxPtr     prect;
{
    register int	x;
    register int	y;
    register BoxPtr     pbox;
    register BoxPtr     pboxEnd;
    int			partIn, partOut;
    int			numRects;

    good(region);
    numRects = REGION_NUM_RECTS(region);
    /* useful optimization */
    if (!numRects || !EXTENTCHECK(&region->extents, prect))
        return(rgnOUT);

    if (numRects == 1)
    {
	/* We know that it must be rgnIN or rgnPART */
	if (SUBSUMES(&region->extents, prect))
	    return(rgnIN);
	else
	    return(rgnPART);
    }

    partOut = FALSE;
    partIn = FALSE;

    /* (x,y) starts at upper left of rect, moving to the right and down */
    x = prect->x1;
    y = prect->y1;

    /* can stop when both partOut and partIn are TRUE, or we reach prect->y2 */
    for (pbox = REGION_BOXPTR(region), pboxEnd = pbox + numRects;
         pbox != pboxEnd;
         pbox++)
    {

        if (pbox->y2 <= y)
           continue;    /* getting up to speed or skipping remainder of band */

        if (pbox->y1 > y)
        {
           partOut = TRUE;      /* missed part of rectangle above */
           if (partIn || (pbox->y1 >= prect->y2))
              break;
           y = pbox->y1;        /* x guaranteed to be == prect->x1 */
        }

        if (pbox->x2 <= x)
           continue;            /* not far enough over yet */

        if (pbox->x1 > x)
        {
           partOut = TRUE;      /* missed part of rectangle to left */
           if (partIn)
              break;
        }

        if (pbox->x1 < prect->x2)
        {
            partIn = TRUE;      /* definitely overlap */
            if (partOut)
               break;
        }

        if (pbox->x2 >= prect->x2)
        {
           y = pbox->y2;        /* finished with this band */
           if (y >= prect->y2)
              break;
           x = prect->x1;       /* reset x out to left again */
        }
	else
	{
	    /*
	     * Because boxes in a band are maximal width, if the first box
	     * to overlap the rectangle doesn't completely cover it in that
	     * band, the rectangle must be partially out, since some of it
	     * will be uncovered in that band. partIn will have been set true
	     * by now...
	     */
	    partOut = TRUE;
	    break;
	}
    }

    return(partIn ? ((y < prect->y2) ? rgnPART : rgnIN) : rgnOUT);
}

/* TranslateRegion(pReg, x, y)
   translates in place
*/

/*
 * XXX: This function is not written as cleanly as it could be (it
 * uses the same rectangle clipping code twice and uses bitwise
 * operators to detect the different region translation cases).
 */

void
miTranslateRegionByBoundary(
                            register RegionPtr pReg,
                            register int x,
                            register int y,
                            int minValue,
                            int maxValue)
{
    int x1, x2, y1, y2;
    register int nbox;
    register BoxPtr pbox;

    good(pReg);
    pReg->extents.x1 = x1 = pReg->extents.x1 + x;
    pReg->extents.y1 = y1 = pReg->extents.y1 + y;
    pReg->extents.x2 = x2 = pReg->extents.x2 + x;
    pReg->extents.y2 = y2 = pReg->extents.y2 + y;
    if (((x1 - minValue)|(y1 - minValue)|(maxValue - x2)|(maxValue - y2)) >= 0)
    {
	if (pReg->data && (nbox = pReg->data->numRects))
	{
	    for (pbox = REGION_BOXPTR(pReg); nbox--; pbox++)
	    {
		pbox->x1 += x;
		pbox->y1 += y;
		pbox->x2 += x;
		pbox->y2 += y;
	    }
	}
	return;
    }
    if (((x2 - minValue)|(y2 - minValue)|(maxValue - x1)|(maxValue - y1)) <= 0)
    {
	pReg->extents.x2 = pReg->extents.x1;
	pReg->extents.y2 = pReg->extents.y1;
	xfreeData(pReg);
	pReg->data = &miEmptyData;
	return;
    }
    if (x1 < minValue)
	pReg->extents.x1 = minValue;
    else if (x2 > maxValue)
	pReg->extents.x2 = maxValue;
    if (y1 < minValue)
	pReg->extents.y1 = minValue;
    else if (y2 > maxValue)
	pReg->extents.y2 = maxValue;
    if (pReg->data && (nbox = pReg->data->numRects))
    {
	register BoxPtr pboxout;

	for (pboxout = pbox = REGION_BOXPTR(pReg); nbox--; pbox++)
	{
	    pboxout->x1 = x1 = pbox->x1 + x;
	    pboxout->y1 = y1 = pbox->y1 + y;
	    pboxout->x2 = x2 = pbox->x2 + x;
	    pboxout->y2 = y2 = pbox->y2 + y;
	    if (((x2 - minValue)|(y2 - minValue)|
		 (maxValue - x1)|(maxValue - y1)) <= 0)
	    {
		pReg->data->numRects--;
		continue;
	    }
	    if (x1 < minValue)
		pboxout->x1 = minValue;
	    else if (x2 > maxValue)
		pboxout->x2 = maxValue;
	    if (y1 < minValue)
		pboxout->y1 = minValue;
	    else if (y2 > maxValue)
		pboxout->y2 = maxValue;
	    pboxout++;
	}
	if (pboxout != pbox)
	{
	    if (pReg->data->numRects == 1)
	    {
		pReg->extents = *REGION_BOXPTR(pReg);
		xfreeData(pReg);
		pReg->data = (RegDataPtr)NULL;
	    }
	    else
		miSetExtents(pReg);
	}
    }
}

/*
 * Translate the region and use the default values which are
 * R_MININT, R_MAXINT for boundary check 
 */

void
miTranslateRegion(
                  register RegionPtr pReg,
                  register int x,
                  register int y)
{
    miTranslateRegionByBoundary(pReg, x, y, R_MININT, R_MAXINT);
}


Bool
miRegionDataCopy(
    register RegionPtr dst,
    register RegionPtr src)
{
    good(dst);
    good(src);
    if (dst->data) 
	return TRUE;
    if (dst == src)
	return TRUE;
    if (!src->data || !src->data->size)
    {
	xfreeData(dst);
	dst->data = (RegDataPtr)NULL;
	return TRUE;
    }
    if (!dst->data || (dst->data->size < src->data->numRects))
    {
	xfreeData(dst);
	dst->data = xallocData(src->data->numRects);
	if (!dst->data)
	    return miRegionBreak (dst);
    }
    dst->data->size = src->data->size;
    dst->data->numRects = src->data->numRects;
    return TRUE;
}

void
miRegionReset(pReg, pBox)
    RegionPtr pReg;
    BoxPtr pBox;
{
    good(pReg);
    ASSERT(pBox->x1<=pBox->x2);
    ASSERT(pBox->y1<=pBox->y2);
    pReg->extents = *pBox;
    xfreeData(pReg);
    pReg->data = (RegDataPtr)NULL;
}

Bool
miPointInRegion(pReg, x, y, box)
    register RegionPtr pReg;
    register int x, y;
    BoxPtr box;     /* "return" value */
{
    register BoxPtr pbox, pboxEnd;
    int numRects;

    good(pReg);
    numRects = REGION_NUM_RECTS(pReg);
    if (!numRects || !INBOX(&pReg->extents, x, y))
        return(FALSE);
    if (numRects == 1)
    {
	*box = pReg->extents;
	return(TRUE);
    }
    for (pbox = REGION_BOXPTR(pReg), pboxEnd = pbox + numRects;
	 pbox != pboxEnd;
	 pbox++)
    {
        if (y >= pbox->y2)
	   continue;		/* not there yet */
	if ((y < pbox->y1) || (x < pbox->x1))
	   break;		/* missed it */
	if (x >= pbox->x2)
	   continue;		/* not there yet */
	*box = *pbox;
	return(TRUE);
    }
    return(FALSE);
}

Bool
miRegionNotEmpty(pReg)
    RegionPtr pReg;
{
    good(pReg);
    return(!REGION_NIL(pReg));
}

Bool
miRegionBroken(RegionPtr pReg)
{
    good(pReg);
    return (REGION_NAR(pReg));
}

void
miRegionEmpty(pReg)
    RegionPtr pReg;
{
    good(pReg);
    xfreeData(pReg);
    pReg->extents.x2 = pReg->extents.x1;
    pReg->extents.y2 = pReg->extents.y1;
    pReg->data = &miEmptyData;
}

BoxPtr
miRegionExtents(pReg)
    RegionPtr pReg;
{
    good(pReg);
    return(&pReg->extents);
}

#define ExchangeSpans(a, b)				    \
{							    \
    DDXPointRec     tpt;				    \
    register int    tw;					    \
							    \
    tpt = spans[a]; spans[a] = spans[b]; spans[b] = tpt;    \
    tw = widths[a]; widths[a] = widths[b]; widths[b] = tw;  \
}

/* ||| I should apply the merge sort code to rectangle sorting above, and see
   if mapping time can be improved.  But right now I've been at work 12 hours,
   so forget it.
*/

static void QuickSortSpans(
    register DDXPointRec    spans[],
    register int	    widths[],
    register int	    numSpans)
{
    register int	    y;
    register int	    i, j, m;
    register DDXPointPtr    r;

    /* Always called with numSpans > 1 */
    /* Sorts only by y, doesn't bother to sort by x */

    do
    {
	if (numSpans < 9)
	{
	    /* Do insertion sort */
	    register int yprev;

	    yprev = spans[0].y;
	    i = 1;
	    do
	    { /* while i != numSpans */
		y = spans[i].y;
		if (yprev > y)
		{
		    /* spans[i] is out of order.  Move into proper location. */
		    DDXPointRec tpt;
		    int	    tw, k;

		    for (j = 0; y >= spans[j].y; j++) {}
		    tpt = spans[i];
		    tw  = widths[i];
		    for (k = i; k != j; k--)
		    {
			spans[k] = spans[k-1];
			widths[k] = widths[k-1];
		    }
		    spans[j] = tpt;
		    widths[j] = tw;
		    y = spans[i].y;
		} /* if out of order */
		yprev = y;
		i++;
	    } while (i != numSpans);
	    return;
	}

	/* Choose partition element, stick in location 0 */
	m = numSpans / 2;
	if (spans[m].y > spans[0].y)		ExchangeSpans(m, 0);
	if (spans[m].y > spans[numSpans-1].y)   ExchangeSpans(m, numSpans-1);
	if (spans[m].y > spans[0].y)		ExchangeSpans(m, 0);
	y = spans[0].y;

        /* Partition array */
        i = 0;
        j = numSpans;
        do
	{
	    r = &(spans[i]);
	    do
	    {
		r++;
		i++;
            } while (i != numSpans && r->y < y);
	    r = &(spans[j]);
	    do
	    {
		r--;
		j--;
            } while (y < r->y);
            if (i < j)
		ExchangeSpans(i, j);
        } while (i < j);

        /* Move partition element back to middle */
        ExchangeSpans(0, j);

	/* Recurse */
        if (numSpans-j-1 > 1)
	    QuickSortSpans(&spans[j+1], &widths[j+1], numSpans-j-1);
        numSpans = j;
    } while (numSpans > 1);
}

#define NextBand()						    \
{								    \
    clipy1 = pboxBandStart->y1;					    \
    clipy2 = pboxBandStart->y2;					    \
    pboxBandEnd = pboxBandStart + 1;				    \
    while (pboxBandEnd != pboxLast && pboxBandEnd->y1 == clipy1) {  \
	pboxBandEnd++;						    \
    }								    \
    for (; ppt != pptLast && ppt->y < clipy1; ppt++, pwidth++) {} \
}

/*
    Clip a list of scanlines to a region.  The caller has allocated the
    space.  FSorted is non-zero if the scanline origins are in ascending
    order.
    returns the number of new, clipped scanlines.
*/

int
miClipSpans(
    RegionPtr		    prgnDst,
    register DDXPointPtr    ppt,
    register int	    *pwidth,
    int			    nspans,
    register DDXPointPtr    pptNew,
    int			    *pwidthNew,
    int			    fSorted)
{
    register DDXPointPtr pptLast;
    int			*pwidthNewStart;	/* the vengeance of Xerox! */
    register int	y, x1, x2;
    register int	numRects;

    good(prgnDst);
    pptLast = ppt + nspans;
    pwidthNewStart = pwidthNew;

    if (!prgnDst->data)
    {
	/* Do special fast code with clip boundaries in registers(?) */
	/* It doesn't pay much to make use of fSorted in this case, 
	   so we lump everything together. */

	register    int clipx1, clipx2, clipy1, clipy2;

	clipx1 = prgnDst->extents.x1;
	clipy1 = prgnDst->extents.y1;
	clipx2 = prgnDst->extents.x2;
	clipy2 = prgnDst->extents.y2;
	    
	for (; ppt != pptLast; ppt++, pwidth++)
	{
	    y = ppt->y;
	    x1 = ppt->x;
	    if (clipy1 <= y && y < clipy2)
	    {
		x2 = x1 + *pwidth;
		if (x1 < clipx1)    x1 = clipx1;
		if (x2 > clipx2)    x2 = clipx2;
		if (x1 < x2)
		{
		    /* part of span in clip rectangle */
		    pptNew->x = x1;
		    pptNew->y = y;
		    *pwidthNew = x2 - x1;
		    pptNew++;
		    pwidthNew++;
		}
	    }
	} /* end for */

    }
    else if ((numRects = prgnDst->data->numRects))
    {
	/* Have to clip against many boxes */
	BoxPtr		pboxBandStart, pboxBandEnd;
	register BoxPtr pbox;
	register BoxPtr pboxLast;
	register int	clipy1, clipy2;

	/* In this case, taking advantage of sorted spans gains more than
	   the sorting costs. */
	if ((! fSorted) && (nspans > 1))
	    QuickSortSpans(ppt, pwidth, nspans);

	pboxBandStart = REGION_BOXPTR(prgnDst);
	pboxLast = pboxBandStart + numRects;
    
	NextBand();

	for (; ppt != pptLast; )
	{
	    y = ppt->y;
	    if (y < clipy2)
	    {
		/* span is in the current band */
		pbox = pboxBandStart;
		x1 = ppt->x;
		x2 = x1 + *pwidth;
		do
		{ /* For each box in band */
		    register int    newx1, newx2;

		    newx1 = x1;
		    newx2 = x2;
		    if (newx1 < pbox->x1)   newx1 = pbox->x1;
		    if (newx2 > pbox->x2)   newx2 = pbox->x2;
		    if (newx1 < newx2)
		    {
			/* Part of span in clip rectangle */
			pptNew->x = newx1;
			pptNew->y = y;
			*pwidthNew = newx2 - newx1;
			pptNew++;
			pwidthNew++;
		    }
		    pbox++;
		} while (pbox != pboxBandEnd);
		ppt++;
		pwidth++;
	    }
	    else
	    {
		/* Move to next band, adjust ppt as needed */
		pboxBandStart = pboxBandEnd;
		if (pboxBandStart == pboxLast)
		    break; /* We're completely done */
		NextBand();
	    }
	}
    }
    return (pwidthNew - pwidthNewStart);
}

/* find the band in a region with the most rectangles */
int
miFindMaxBand(prgn)
    RegionPtr prgn;
{
    register int nbox;
    register BoxPtr pbox;
    register int nThisBand;
    register int nMaxBand = 0;
    int      yThisBand;

    good(prgn);
    nbox = REGION_NUM_RECTS(prgn);
    pbox = REGION_RECTS(prgn);

    while(nbox > 0)
    {
	yThisBand = pbox->y1;
	nThisBand = 0;
	while((nbox > 0) && (pbox->y1 == yThisBand))
	{
	    nbox--;
	    pbox++;
	    nThisBand++;
	}
	if (nThisBand > nMaxBand)
	    nMaxBand = nThisBand;
    }
    return (nMaxBand);
}


/*
 *----------------------------------------------------------------------------
 *
 * miApplyRect --
 *
 *      This is a simple helper function for use with the common case of
 *      (newreg = oldreg (op) newrect) where (op) is one of miUnion,
 *      miSubtract, miIntersect.
 *
 * Results:
 *      TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *      Modifies newReg.
 *
 *----------------------------------------------------------------------------
 */

Bool
miApplyRect(RegionPtr newReg, RegionPtr reg, BoxPtr rect,
            Bool (*op) (RegionPtr, RegionPtr, RegionPtr))
{
   RegionRec topReg;

   good(reg);

   ASSERT(RECTINFO_ISVALID(rect->info));

   if (rect->x2 <= rect->x1 || rect->y2 <= rect->y1) {
      rect->x2 = rect->x1 = rect->y2 = rect->y1 = 0;
      topReg.data = &miEmptyData;
   } else {
      topReg.data = NULL;
   }

   topReg.extents = *rect;

   return (op)(newReg, reg, &topReg);
}


/*
 *----------------------------------------------------------------------------
 *
 * miRegionArea --
 *
 *      Returns the total actual area covered by all the rectangles in the
 *      region. Note that this is not the same as the area of the bounding
 *      box, obviously.
 *
 *      XXX *could* calculate this iteratively and try to keep it up to date.
 *
 *----------------------------------------------------------------------------
 */

int
miRegionArea(RegionPtr reg)
{
   int i, area = 0;

   for (i = 0; i < REGION_NUM_RECTS(reg); i++) {
      BoxPtr rect = &REGION_RECTS(reg)[i];
      ASSERT(rect);
      area += (rect->x2 - rect->x1) * (rect->y2 - rect->y1);
   }

   return area;
}


/*
 *----------------------------------------------------------------------------
 *
 * miRegionMatch --
 *
 *      Sets newReg to be a region containing all rects in reg for which
 *      match(rect) returns TRUE.
 *
 * Results:
 *      Returns TRUE if there were any matches.
 *
 * Side effects:
 *      Modifies newReg.
 *
 *----------------------------------------------------------------------------
 */

Bool
miRegionMatch(RegionPtr newReg, RegionPtr reg,
              miRegionMatchFunc match, uintptr_t userData)
{
   int i;
   Bool ret = FALSE;
   RegionRec tmp;

   /*
    * Use a tmp region in case newReg and reg point to the same region.
    */
   miRegionInit(&tmp, NullBox, 0);

   for (i = 0; i < REGION_NUM_RECTS(reg); i++) {
      BoxPtr rect = REGION_RECTS(reg) + i;
      if (match(rect, userData)) {
         miApplyRect(&tmp, &tmp, rect, miUnion);
         ret = TRUE;
      }
   }

   miRegionCopy(newReg, &tmp);

   miRegionUninit(&tmp);

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * miIsRegionVoid --
 *
 *      Checks if the region passes the REGION_NIL test.  If it doesn't
 *      pass, checks if the region's extents rectangle has 0 size.
 *
 * Results:
 *      TRUE if the region is void, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
miIsRegionVoid(RegionPtr reg) // IN
{
   return    REGION_NIL(reg)
          || (   reg->data == NULL
              && reg->extents.x1 == reg->extents.x2
              && reg->extents.y1 == reg->extents.y2);
}
