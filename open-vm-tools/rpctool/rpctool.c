/*********************************************************
 * Copyright (C) 2002-2019 VMware, Inc. All rights reserved.
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

#ifndef _WIN32
#include "sigPosixRegs.h"
#include <errno.h>
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vmware.h"
#include "rpcout.h"
#include "str.h"
#include "backdoor_def.h"
#ifdef _WIN32
#include "vmware/tools/win32util.h"
#endif

#define NOT_VMWARE_ERROR "Failed sending message to VMware.\n"

int RpcToolCommand(int argc, char *argv[]);

#ifndef _WIN32
static Bool SetSignalHandler(int sig,
                             void (*handler)(int, siginfo_t *, void *),
                             Bool reset);
#endif

void
PrintUsage(void)
{
   fprintf(stderr, "rpctool syntax:\n\n");
   fprintf(stderr, "  -h | --help\tprint usage.\n");
   fprintf(stderr, "  rpctool <text>\tsend <text> as an RPC command.\n");
}


#ifdef _WIN32
static Bool
ExceptionIsBackdoor(PEXCEPTION_POINTERS excInfo)
{
#ifdef _WIN64
   uint32 magic = excInfo->ContextRecord->Rax & 0xffffffff;
   uint16 port = excInfo->ContextRecord->Rdx & 0xffff;
#else
   uint32 magic = excInfo->ContextRecord->Eax & 0xffffffff;
   uint16 port = excInfo->ContextRecord->Edx & 0xffff;
#endif

   return (magic == BDOOR_MAGIC &&
           (port == BDOOR_PORT || port == BDOORHB_PORT));
}


#else
static void
SignalHandler(int sig,
              siginfo_t *sip,
              void *data)
{
   ucontext_t *ucp = (ucontext_t *) data;
   uint16 port = SC_EDX(ucp) & 0xffff;
   uint32 magic = SC_EAX(ucp) & 0xffffffff;

   if (magic == BDOOR_MAGIC &&
       (port == BDOORHB_PORT || port == BDOOR_PORT)) {
      fprintf(stderr, NOT_VMWARE_ERROR);
      exit(1);
   } else {
      SetSignalHandler(sig, NULL, TRUE);
      raise(sig);
   }
}


static Bool
SetSignalHandler(int sig,
                 void (*handler)(int, siginfo_t *, void *),
                 Bool reset)
{
   static struct sigaction old;

   if (reset) {
      struct sigaction tmp = old;

      memset(&old, 0, sizeof old);
      return sigaction(sig, &tmp, NULL) ? FALSE : TRUE;
   } else {
      struct sigaction new;

      /* Setup the handler and flags to get exception information. */
      new.sa_sigaction = handler;
      new.sa_flags = SA_SIGINFO;

      /* Block all signals when handling this. */
      if (sigfillset(&new.sa_mask) == -1) {
         fprintf(stderr, "Unable to initialize a signal set: %s.\n\n",
                 strerror(errno));
         return FALSE;
      }

      if (sigaction(sig, &new, &old) == -1) {
         fprintf(stderr, "Unable to initialize a signal handler: %s.\n\n",
                 strerror(errno));
         return FALSE;
      }
   }
   return TRUE;
}
#endif


int
main(int argc, char *argv[])
{
   int ret = 1;

#ifdef _WIN32
   WinUtil_EnableSafePathSearching(TRUE);
#endif

   if (argc <= 1) {
      PrintUsage();
      return 1;
   }

   if (!strncmp(argv[1], "-h", 2) ||
       !strncmp(argv[1], "--help", 6)) {
      PrintUsage();
      return 0;
   }

   argc--;
   argv++;

#ifdef _WIN32
   __try {
      ret = RpcToolCommand(argc, argv);
   } __except(ExceptionIsBackdoor(GetExceptionInformation()) ?
               EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
      fprintf(stderr, NOT_VMWARE_ERROR);
      return 1;
   }
#else
#ifdef __FreeBSD__
#  define ERROR_SIGNAL SIGBUS
#else
#  define ERROR_SIGNAL SIGSEGV
#endif
   if (SetSignalHandler(ERROR_SIGNAL, SignalHandler, FALSE)) {
      ret = RpcToolCommand(argc, argv);
   }
   SetSignalHandler(ERROR_SIGNAL, NULL, TRUE);
#endif
   return ret;
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


