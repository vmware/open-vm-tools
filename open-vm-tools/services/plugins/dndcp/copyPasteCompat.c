/*********************************************************
 * Copyright (C) 2011-2016 VMware, Inc. All rights reserved.
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
 * copyPasteCompat.c --
 *
 *    Legacy copy/paste functions.
 */

#include "backdoor.h"
#include "backdoor_def.h"
#include "copyPasteCompat.h"

/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste_GetHostSelectionLen --
 *
 *      Retrieve the length of the clipboard (if any) to receive from the
 *      VMX.
 *
 * Results:
 *      Length >= 0 if a clipboard must be retrieved from the host.
 *      < 0 on error (VMWARE_DONT_EXCHANGE_SELECTIONS or
 *                    VMWARE_SELECTION_NOT_READY currently)
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

int32
CopyPaste_GetHostSelectionLen(void)
{
   Backdoor_proto bp;

   bp.in.cx.halfs.low = BDOOR_CMD_GETSELLENGTH;
   Backdoor(&bp);
   return bp.out.ax.word;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPasteGetNextPiece --
 *
 *      Retrieve the next 4 bytes of the host clipboard.
 *
 * Results:
 *      The next 4 bytes of the host clipboard.
 *
 * Side effects:
 *	     None
 *
 *-----------------------------------------------------------------------------
 */

static uint32
CopyPasteGetNextPiece(void)
{
   Backdoor_proto bp;

   bp.in.cx.halfs.low = BDOOR_CMD_GETNEXTPIECE;
   Backdoor(&bp);
   return bp.out.ax.word;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste_GetHostSelection --
 *
 *      Retrieve the host clipboard. 'data' must be a buffer whose size is at
 *      least (('size' + 4 - 1) / 4) * 4 bytes.
 *
 * Results:
 *      The host clipboard in 'data'.
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPaste_GetHostSelection(unsigned int size, // IN
                           char *data)        // OUT
{
   uint32 *current;
   uint32 const *end;

   current = (uint32 *)data;
   end = current + (size + sizeof *current - 1) / sizeof *current;
   for (; current < end; current++) {
      *current = CopyPasteGetNextPiece();
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste_SetSelLength --
 *
 *      Tell the VMX about the length of the clipboard we are about to send
 *      to it.
 *
 * Results:
 *      None
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPaste_SetSelLength(uint32 length) // IN
{
   Backdoor_proto bp;

   bp.in.cx.halfs.low = BDOOR_CMD_SETSELLENGTH;
   bp.in.size = length;
   Backdoor(&bp);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CopyPaste_SetNextPiece --
 *
 *      Send the next 4 bytes of the guest clipboard.
 *
 * Results:
 *      None
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

void
CopyPaste_SetNextPiece(uint32 data) // IN
{
   Backdoor_proto bp;

   bp.in.cx.halfs.low = BDOOR_CMD_SETNEXTPIECE;
   bp.in.size = data;
   Backdoor(&bp);
}

