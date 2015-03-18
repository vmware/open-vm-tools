/*********************************************************
 * Copyright (C) 2011 VMware, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *********************************************************/


/*
 * os_panic.c --
 *
 *     Vararg panic implementation for FreeBSD.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/stdarg.h>

#include "os.h"

/*
 *----------------------------------------------------------------------------
 *
 * os_panic --
 *
 *    FreeBSD panic implementation that takes va_list argument.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    We panic.
 *
 *----------------------------------------------------------------------------
 */

void
os_panic(const char *fmt,  // IN
         va_list args)     // IN
{
   static char message[1024];

   vsnprintf(message, sizeof message - 1, fmt, args);
   message[sizeof message - 1] = '\0';

   panic("%s", message);
}
