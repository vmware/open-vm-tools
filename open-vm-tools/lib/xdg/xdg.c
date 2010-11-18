/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>

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
 *      success or NULL on failure.
 *
 *      This function only guarantees that the returned string matches the
 *      pattern ^[A-Z]*$.
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

         if (StdIO_ReadNextLine(cmdPipe, &outbuf, maxSize, &outLen)
             == StdIO_Success) {
            char *i;

            for (i = outbuf; i < &outbuf[outLen]; i++) {
               /* We expect a string in all capitals. */
               if (*i < 'A' || *i > 'Z') {
                  free(outbuf);
                  outbuf = NULL;
                  break;
               }
            }
         }

         status = pclose(cmdPipe);
         if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            free(outbuf);
            outbuf = NULL;
         }
      }
   }

   return outbuf;
}
