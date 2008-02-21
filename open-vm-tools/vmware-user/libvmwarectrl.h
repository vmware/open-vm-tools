/*
 * Copyright 2006 by VMware, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the copyright holder(s)
 * and author(s) shall not be used in advertising or otherwise to promote
 * the sale, use or other dealings in this Software without prior written
 * authorization from the copyright holder(s) and author(s).
 */

/*
 * libvmwarectrl.c --
 *
 *      The VMWARE_CTRL client library.
 */


#ifndef _LIB_VMWARE_CTRL_H_
#define _LIB_VMWARE_CTRL_H_

#include <X11/X.h>
#include <X11/Xmd.h>
#ifndef NO_MULTIMON
#include <X11/extensions/panoramiXproto.h>
#endif

Bool VMwareCtrl_QueryExtension(Display *dpy, int *event_basep, int *error_basep);
Bool VMwareCtrl_QueryVersion(Display *dpy, int *majorVersion, int *minorVersion);
Bool VMwareCtrl_SetRes(Display *dpy, int screen, int x, int y);
#ifndef  NO_MULTIMON
Bool VMwareCtrl_SetTopology(Display *dpy, int screen, xXineramaScreenInfo[], int number);
#endif

#endif /* _LIB_VMWARE_CTRL_H_ */
