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
 * rpcTool.c --
 *
 *      Simple program to drive the guest RPC library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vmware.h"
#include "rpcout.h"
#include "str.h"

int RpcToolCommand(int argc, char *argv[]);

void
PrintUsage(void)
{
   fprintf(stderr, "rpctool syntax:\n\n");
   fprintf(stderr, "  rpctool <text>\n\n");
}


int
main(int argc, char *argv[])
{
   if (argc <= 1) {
      PrintUsage();
      return 1;
   }

   argc--;
   argv++;

   return RpcToolCommand(argc, argv);
}

int
RpcToolCommand(int argc, char *argv[])
{
   char *result = NULL;
   Bool status = FALSE;

   status = RpcOut_sendOne(&result, NULL, "%s", argv[0]);
   if (!status) {
      fprintf(stderr, "%s\n", result ? result : "NULL");
   } else {
      printf("%s\n", result);
   }
   free(result);
   return (status == TRUE ? 0 : 1);
}

void
Panic(const char *fmt, ...)
{
   va_list args;
   static char buf[1024];

   va_start(args, fmt);
   Str_Vsnprintf(buf, sizeof buf, fmt, args);
   va_end(args);

   fputs(buf, stderr);
   abort();
}


void
Debug(char *fmt, ...)
{
#if defined(VMX86_DEBUG) || defined(VMX86_DEVEL)
   va_list args;
   char *buf;

   va_start(args, fmt);
   buf = Str_Vasprintf(NULL, fmt, args);
   va_end(args);

   fprintf(stderr, "rpctool: %s\n", buf);
   fflush(stderr);
#else
   /* No output in non-debug/non-developer builds */
#endif
}


