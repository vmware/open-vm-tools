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
 * vmwarectrlproto.h --
 *
 *      The description of the VMWARE_CTRL protocol extension that
 *      allows X clients to communicate with the driver.
 */

#ifndef _VMWARE_CTRL_PROTO_H_
#define _VMWARE_CTRL_PROTO_H_


#include <X11/X.h>
#include "vmwarectrl.h"


/*
 * Requests and Replies
 */

/* Version 0.1 definitions. */

typedef struct {
   CARD8  reqType;           /* always X_VMwareCtrlReqCode */
   CARD8  VMwareCtrlReqType; /* always X_VMwareCtrlQueryVersion */
   CARD16 length B16;
   CARD32 majorVersion B32;
   CARD32 minorVersion B32;
} xVMwareCtrlQueryVersionReq;
#define sz_xVMwareCtrlQueryVersionReq 12

typedef struct {
   BYTE    type; /* X_Reply */
   BYTE    pad1;
   CARD16  sequenceNumber B16;
   CARD32  length B32;
   CARD32  majorVersion B32;
   CARD32  minorVersion B32;
   CARD32  pad2 B32;
   CARD32  pad3 B32;
   CARD32  pad4 B32;
   CARD32  pad5 B32;
} xVMwareCtrlQueryVersionReply;
#define sz_xVMwareCtrlQueryVersionReply 32

typedef struct {
   CARD8  reqType;           /* always X_VMwareCtrlReqCode */
   CARD8  VMwareCtrlReqType; /* always X_VMwareCtrlSetRes */
   CARD16 length B16;
   CARD32 screen B32;
   CARD32 x B32;
   CARD32 y B32;
} xVMwareCtrlSetResReq;
#define sz_xVMwareCtrlSetResReq 16

typedef struct {
   BYTE   type; /* X_Reply */
   BYTE   pad1;
   CARD16 sequenceNumber B16;
   CARD32 length B32;
   CARD32 screen B32;
   CARD32 x B32;
   CARD32 y B32;
   CARD32 pad2 B32;
   CARD32 pad3 B32;
   CARD32 pad4 B32;
} xVMwareCtrlSetResReply;
#define sz_xVMwareCtrlSetResReply 32

/* Version 0.2 definitions. */

typedef struct {
   CARD8  reqType;           /* always X_VMwareCtrlReqCode */
   CARD8  VMwareCtrlReqType; /* always X_VMwareCtrlSetTopology */
   CARD16 length B16;
   CARD32 screen B32;
   CARD32 number B32;
   CARD32 pad1   B32;
} xVMwareCtrlSetTopologyReq;
#define sz_xVMwareCtrlSetTopologyReq 16

typedef struct {
   BYTE   type; /* X_Reply */
   BYTE   pad1;
   CARD16 sequenceNumber B16;
   CARD32 length B32;
   CARD32 screen B32;
   CARD32 pad2   B32;
   CARD32 pad3   B32;
   CARD32 pad4   B32;
   CARD32 pad5   B32;
   CARD32 pad6   B32;
} xVMwareCtrlSetTopologyReply;
#define sz_xVMwareCtrlSetTopologyReply 32

#endif /* _VMWARE_CTRL_PROTO_H_ */
