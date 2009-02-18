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


#define NEED_EVENTS
#define NEED_REPLIES
#include <X11/Xlibint.h>
#include "libvmwarectrl.h"
#include "vmwarectrlproto.h"
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>


/*
 * Static data and functions.
 */
 
static XExtensionInfo _vmwarectrl_info_data;
static XExtensionInfo *vmwarectrl_info = &_vmwarectrl_info_data;
static char *vmwarectrl_extension_name = VMWARE_CTRL_PROTOCOL_NAME;

#define VMwareCtrlCheckExtension(dpy, i, val) \
  XextCheckExtension(dpy, i, vmwarectrl_extension_name, val)

static int close_display(Display *dpy, XExtCodes *codes);

static /* const */ XExtensionHooks vmwarectrl_extension_hooks = {
    NULL,          /* create_gc */
    NULL,          /* copy_gc */
    NULL,          /* flush_gc */
    NULL,          /* free_gc */
    NULL,          /* create_font */
    NULL,          /* free_font */
    close_display, /* close_display */
    NULL,          /* wire_to_event */
    NULL,          /* event_to_wire */
    NULL,          /* error */
    NULL,          /* error_string */
};

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, vmwarectrl_info)

static XEXT_GENERATE_FIND_DISPLAY (find_display, vmwarectrl_info,
				   vmwarectrl_extension_name,
				   &vmwarectrl_extension_hooks,
				   0, NULL)

/*
 *----------------------------------------------------------------------------
 *
 * VMwareCtrl_QueryExtension --
 *
 *      Standard QueryExtension implementation. Not very interesting for
 *      VMWARE_CTRL as it doesn't define any events or errors.
 *
 * Results:
 *      True if information is successfully retrieved. False otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
VMwareCtrl_QueryExtension(Display *dpy,     // IN:
                          int *event_basep, // OUT:
                          int *error_basep) // OUT:
{
   XExtDisplayInfo *info = find_display(dpy);

   if (XextHasExtension(info)) {
      *event_basep = info->codes->first_event;
      *error_basep = info->codes->first_error;
      return True;
   } else {
      return False;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * VMwareCtrl_QueryVersion --
 *
 *      Send the QueryVersion command to the driver and return the results.
 *
 * Results:
 *      True if information is successfully retrieved. False otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
VMwareCtrl_QueryVersion(Display *dpy,      // IN:
                        int *majorVersion, // OUT:
                        int *minorVersion) // OUT:
{
   xVMwareCtrlQueryVersionReply rep;
   xVMwareCtrlQueryVersionReq *req;
   XExtDisplayInfo *info = find_display(dpy);
   Bool ret = False;

   VMwareCtrlCheckExtension(dpy, info, False);
   LockDisplay(dpy);

   GetReq(VMwareCtrlQueryVersion, req);
   req->reqType = info->codes->major_opcode;
   req->VMwareCtrlReqType = X_VMwareCtrlQueryVersion;

   if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
      goto exit;
   }
   *majorVersion = rep.majorVersion;
   *minorVersion = rep.minorVersion;

   ret = True;

exit:
   UnlockDisplay(dpy);
   SyncHandle();

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMwareCtrl_SetRes --
 *
 *      Send the SetRes command to the driver.
 *
 * Results:
 *      True if the resolution was set. False otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
VMwareCtrl_SetRes(Display *dpy, // IN:
                  int screen,   // IN:
                  int x,        // IN:
                  int y)        // IN:
{
   xVMwareCtrlSetResReply rep;
   xVMwareCtrlSetResReq *req;
   XExtDisplayInfo *info = find_display(dpy);
   Bool ret = False;

   VMwareCtrlCheckExtension(dpy, info, False);
   LockDisplay(dpy);

   GetReq(VMwareCtrlSetRes, req);
   req->reqType = info->codes->major_opcode;
   req->VMwareCtrlReqType = X_VMwareCtrlSetRes;
   req->screen = screen;
   req->x = x;
   req->y = y;

   if (!_XReply(dpy, (xReply *)&rep,
                (SIZEOF(xVMwareCtrlSetResReply) - SIZEOF(xReply)) >> 2,
                xFalse)) {
      goto exit;
   }

   ret = True;

exit:
   UnlockDisplay(dpy);
   SyncHandle();

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMwareCtrl_SetTopology --
 *
 *      Send the SetTopology command to the driver.
 *
 *      Solaris 10 uses a different Xinerama standard than expected here. As a
 *      result, topology set is not supported and this function is excluded from
 *      Solaris builds.
 *
 * Results:
 *      True if the resolution and xinerama topology were set. False otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

#ifndef NO_MULTIMON
Bool
VMwareCtrl_SetTopology(Display *dpy,          // IN:
                       int screen,            // IN:
                       xXineramaScreenInfo extents[], // IN:
                       int number)            // IN:
{
   xVMwareCtrlSetTopologyReply rep;
   xVMwareCtrlSetTopologyReq *req;
   XExtDisplayInfo *info = find_display(dpy);
   Bool ret = False;
   long len;

   VMwareCtrlCheckExtension(dpy, info, False);
   LockDisplay(dpy);

   GetReq(VMwareCtrlSetTopology, req);
   req->reqType = info->codes->major_opcode;
   req->VMwareCtrlReqType = X_VMwareCtrlSetTopology;
   req->screen = screen;
   req->number = number;

   len = ((long) number) << 1;
   SetReqLen(req, len, len);
   len <<= 2;
   _XSend(dpy, (char *)extents, len);

   if (!_XReply(dpy, (xReply *)&rep,
                (SIZEOF(xVMwareCtrlSetTopologyReply) - SIZEOF(xReply)) >> 2,
                xFalse)) {
      goto exit;
   }

   ret = True;

exit:
   UnlockDisplay(dpy);
   SyncHandle();

   return ret;
}
#endif
