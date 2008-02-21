/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * stub.c --
 *
 *   Stub for unuseful functions. Stolen from one of the other
 *   13 stub.c files in bora. This is necessary because the 
 *   toolbox uses FileIO_* API, which in turn uses these suckers
 *   below.
 *   --Ganesh.
 *
 */

#ifndef VMX86_DEVEL

#endif 


#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>
#include <string.h>

#ifdef _MSC_VER
#   include <io.h>
#   include <windows.h>
#endif


#include "vm_version.h"
#include "vm_assert.h"
#include "vmware.h"
#include "str.h"
#include "debug.h"

#ifdef _WIN32
#include "poll.h"
#endif

#if defined(N_PLAT_NLM)
char *
File_GetTmpDir(Bool useConf)
{
   return NULL;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * StubVprintf --
 *
 *      Output error text.
 *
 * Results:
 *      
 *      None.
 *
 * Side effects:
 *
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
StubVprintf(const char *prefix,
            const char *fmt,
            va_list args)
{
   char *str;

   /* Doesn't work yet. [greg] */

   str = Str_Vasprintf(NULL, fmt, args);

#if defined(_WIN32)
   Debug("%s: %s", prefix, str);
   printf("%s: %s", prefix, str); 
      // how do we print to stderr? fprintf(stderr,...) doesn't work
#else
   fprintf(stderr, "%s: %s", prefix, str);
   fflush(stderr);
#endif

   free(str);
}

#ifdef N_PLAT_NLM
void 
Panic(const char *fmt, ...)
{
   va_list args;

   va_start(args, fmt);
   StubVprintf("PANIC", fmt, args);
   va_end(args);

   exit(255);
   NOT_REACHED();
}
#endif

void 
Panic_PostPanicMsg(const char *format,
                   ...)
{}

void
Log_DisableThrottling(void)
{}

void
Log_SetAlwaysKeep(Bool unused)
{}


Bool
Config_GetBool(Bool defaultValue,
               const char *fmt,
               ...)
{
   return defaultValue;
}


int32
Config_GetLong(int32 defaultValue,
               const char *fmt,
               ...)
{
   return defaultValue;
}


void 
Log(const char *fmt,
    ...)
{
   va_list args;

   va_start(args, fmt);
   StubVprintf("Log", fmt, args);
   va_end(args);
}

void
Warning(const char *fmt,
        ...)
{
   va_list args;

   va_start(args, fmt);
   StubVprintf("Warning", fmt, args);
   va_end(args);
}


void 
Msg_Append(const char *idFmt, 
           ...)
{
   char *str;
   
   va_list args;
   va_start(args, idFmt);
   str = Str_Vasprintf(NULL, idFmt, args);
   va_end(args);

   Warning("Msg_Append: %s\n", str);
   free(str);
}

unsigned int 
Msg_Question(void *buttons,
             int defaultAnswer,
             char const *fmt,
             ...)
{
   char *str;
   
   va_list args;
   va_start(args, fmt);
   str = Str_Vasprintf(NULL, fmt, args);
   va_end(args);

   Warning("Msg_Question: %s\n", str);
   free(str);

   return 0;
}

typedef int MsgSeverity;

void 
Msg_Post(MsgSeverity severity,
         const char *idFmt,
         ...)
{
   char *str;
   
   va_list args;
   va_start(args, idFmt);
   str = Str_Vasprintf(NULL, idFmt, args);
   va_end(args);

   Warning("Msg_Post: %s\n", str);
   free(str);
}


#ifdef _WIN32
Bool
Preference_GetBool(Bool defaultValue, 
                   const char *name) 
{
   return defaultValue;
}
#endif

char *
Preference_GetString(char *defaultValue, 
                     const char *name) 
{
   return defaultValue;
}

#ifdef _WIN32
VMwareStatus
Poll_CB_RTime(PollerFunction f,           // IN
              void *clientData,           // IN
              int info,                   // IN
              Bool periodic,              // IN
              struct DeviceLock *lock)    // IN
{
   //NOT_IMPLEMENTED();
   return VMWARE_STATUS_SUCCESS;
}


Bool
Poll_CB_RTimeRemove(PollerFunction f,  // IN
                    void *clientData,  // IN
                    Bool periodic)     // IN
{
   //NOT_IMPLEMENTED();
   return TRUE;
}
#endif

