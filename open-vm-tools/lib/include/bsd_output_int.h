/* **********************************************************
 * Copyright 2006 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*-
 * Copyright (c) 1990, 1993
 *   The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

/*
 * bsd_output_int.h --
 *
 *    Declarations private to the BSD-borrowed formatted output
 *    funtions.
 */

#ifndef _BSD_OUTPUT_INT_H_
#define _BSD_OUTPUT_INT_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "bsd_output.h"

#ifdef _WIN32 // {

#pragma warning(disable : 4018 4047 4101 4102 4146 4244 4267)

#define INTMAX_MAX 9223372036854775807i64

typedef unsigned int u_int;
typedef unsigned long u_long;
typedef unsigned short u_short;
typedef unsigned char u_char;
typedef __int64 intmax_t;
typedef unsigned __int64 uintmax_t;
typedef intptr_t ptrdiff_t;

#endif // }

#define MAXEXPDIG 6

/* For u_int and u_long, and other types we might want. */
#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <stddef.h>
#endif

union arg {
   int   intarg;
   u_int   uintarg;
   long   longarg;
   u_long   ulongarg;
   long long longlongarg;
   unsigned long long ulonglongarg;
   ptrdiff_t ptrdiffarg;
   size_t   sizearg;
   intmax_t intmaxarg;
   uintmax_t uintmaxarg;
   void   *pvoidarg;
   char   *pchararg;
   signed char *pschararg;
   short   *pshortarg;
   int   *pintarg;
   long   *plongarg;
   long long *plonglongarg;
   ptrdiff_t *pptrdiffarg;
   size_t   *psizearg;
   intmax_t *pintmaxarg;
#ifndef NO_FLOATING_POINT
   double   doublearg;
   long double longdoublearg;
#endif
   wint_t   wintarg;
   wchar_t   *pwchararg;
};

/*
 * Type ids for argument type table.
 */
enum typeid {
   T_UNUSED, TP_SHORT, T_INT, T_U_INT, TP_INT,
   T_LONG, T_U_LONG, TP_LONG, T_LLONG, T_U_LLONG, TP_LLONG,
   T_PTRDIFFT, TP_PTRDIFFT, T_SIZET, TP_SIZET,
   T_INTMAXT, T_UINTMAXT, TP_INTMAXT, TP_VOID, TP_CHAR, TP_SCHAR,
   T_DOUBLE, T_LONG_DOUBLE, T_WINT, TP_WCHAR
};

/*
 * I/O descriptors for __sfvwrite().
 */
struct __siov {
   void   *iov_base;
   size_t   iov_len;
};
struct __suio {
   struct   __siov *uio_iov;
   int   uio_iovcnt;
   int   uio_resid;
};

#ifndef NO_FLOATING_POINT

#include <float.h>
#include <math.h>

#define DEFPREC 6

extern char * dtoa(double d, int mode, int prec, int *expOut,
                   int *sign, char **strEnd);

extern char * ldtoa(long double *ld, int mode, int prec, int *expOut,
                    int *sign, char **strEnd);

extern void freedtoa(void *mem);

#endif /* !NO_FLOATING_POINT */

#if defined _MSC_VER && _MSC_VER < 1400
/* VC80 has an internal wmemchr */
extern const wchar_t *wmemchr(
   const wchar_t * buf, 
   wchar_t c,
   size_t count
);
#endif

extern wint_t
bsd_btowc(int c);

#endif // _BSD_OUTPUT_INT_H_
