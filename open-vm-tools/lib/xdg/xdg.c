/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * xdg.c --
 *
 *	vmware-xdg-* script wrapper library.
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include "vmware.h"
#include "vmstdio.h"
#include "xdg.h"


/*
 * Local data
 */


/* Name of helper script used by Xdg_DetectDesktopEnv. */
static const char xdgDetectDEExec[] = "vmware-xdg-detect-de";


/*
 * Global functions
 */


/*
 *-----------------------------------------------------------------------------
 *
 * Xdg_DetectDesktopEnv --
 *
 *      Captures output from external vmware-xdg-detect-de script to determine
 *      which desktop environment we're running under.
 *
 * Results:
 *      Returns a pointer to a string specifying the desktop environment on
 *      success or "" on failure.
 *
 *      This function only guarantees that the returned string matches the
 *      pattern ^[A-Za-z0-9]*$.
 *
 * Side effects:
 *      Allocates memory for outbuf on first call for duration of program.
 *      Uses popen(), relying on $PATH, to find and execute xdgDetectDeExec.
 *      Caller must not modify returned string.
 *
 *-----------------------------------------------------------------------------
 */

const char *
Xdg_DetectDesktopEnv(void)
{
   static char *outbuf = NULL;

   if (outbuf == NULL) {
      FILE *cmdPipe = popen(xdgDetectDEExec, "r");

      if (cmdPipe) {
         static const size_t maxSize = sizeof "TEHLONGISTDESKTOPENVEVAR";
         size_t outLen;         // Doesn't include NUL.
         int status;

         if (   StdIO_ReadNextLine(cmdPipe, &outbuf, maxSize, &outLen)
             == StdIO_Success) {
            int i;

            for (i = 0; i < outLen; i++) {
               if (!isalnum(outbuf[i])) {
                  g_debug("%s: received malformed input\n", __func__);
                  free(outbuf);
                  outbuf = NULL;
                  break;
               }
            }
         }

         status = pclose(cmdPipe);
         if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            g_debug("%s: %s did not exit cleanly (%x/%x)\n", __func__, xdgDetectDEExec,
                    status, WEXITSTATUS(status));
            free(outbuf);
            outbuf = NULL;
         }
      }

      if (outbuf == NULL) {
         outbuf = "";
      }
   }

   return outbuf;
}
