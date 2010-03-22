/* **********************************************************
 * Copyright 2008 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
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
 * bsdfmt.h --
 *
 *	BSD-derived formatter (sprintf, etc.) support.
 *
 * Most of this code came from bsd_vsnprintf.c and bsd_output_int.h,
 * which in turn came from vfprintf.c in the FreeBSD distribution.
 * See bsd_vsnprintf.c for more details.
 */

#ifndef _BSDFMT_H_
#define _BSDFMT_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"


#ifdef _WIN32 // {

#pragma warning(disable : 4018 4047 4101 4102 4146 4244 4267)

#define INTMAX_MAX MAX_INT64

typedef unsigned int u_int;
typedef unsigned long u_long;
typedef unsigned short u_short;
typedef unsigned char u_char;
typedef __int64 intmax_t;
typedef unsigned __int64 uintmax_t;
typedef intptr_t ptrdiff_t;

#else // } {

/* For u_int and u_long, and other types we might want. */
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <stddef.h>

#if defined(__FreeBSD__) && __FreeBSD_version < 500029
#define INTMAX_MAX   9223372036854775807LL
#define UINTMAX_MAX  18446744073709551615ULL
typedef int64        intmax_t;
typedef uint64       uintmax_t;
typedef int32        wint_t;
#endif

#endif // }

/*
 * I/O descriptors for BSDFmt_sfvwrite().
 */

typedef struct BSDFmt_IOV {
   void *iov_base;
   size_t iov_len;
} BSDFmt_IOV;

typedef struct BSDFmt_UIO {
   BSDFmt_IOV *uio_iov;
   int uio_iovcnt;
   int uio_resid;
} BSDFmt_UIO;

#define BSDFMT_NIOV 8

typedef struct BSDFmt_StrBuf {
   Bool alloc;
   Bool error;
   char *buf;
   size_t size;
   size_t index;
} BSDFmt_StrBuf;

int BSDFmt_SFVWrite(BSDFmt_StrBuf *sbuf, BSDFmt_UIO *uio);
int BSDFmt_SPrint(BSDFmt_StrBuf *sbuf, BSDFmt_UIO *uio);


/*
 * Conversion functions
 */

char *BSDFmt_WCharToUTF8(wchar_t *, int);
char *BSDFmt_UJToA(uintmax_t, char *, int, int, const char *, int, char,
                   const char *);


/*
 * Don't use typedef for mbstate_t because it's actually defined
 * in VS2003/VC7/include/wchar.h -- edward
 */

#ifdef _WIN32
#define mbstate_t int
#endif


/*
 * Macros for converting digits to letters and vice versa
 */

#define to_digit(c)   ((c) - '0')
#define is_digit(c)   ((unsigned)to_digit(c) <= 9)
#define to_char(n)    ((n) + '0')


/*
 * Floating point
 */

#ifndef NO_FLOATING_POINT // {

#include <float.h>
#include <math.h>

#define MAXEXPDIG 6
#define DEFPREC 6

int BSDFmt_Exponent(char *, int, int);

extern char *dtoa(double d, int mode, int prec, int *expOut,
                  int *sign, char **strEnd);
extern char *ldtoa(long double *ld, int mode, int prec, int *expOut,
                   int *sign, char **strEnd);
extern void freedtoa(void *mem);

#endif // }


/*
 * The size of the buffer we use as scratch space for integer
 * conversions, among other things.  Technically, we would need the
 * most space for base 10 conversions with thousands' grouping
 * characters between each pair of digits.  100 bytes is a
 * conservative overestimate even for a 128-bit uintmax_t.
 */

#define INT_CONV_BUF 100

#define STATIC_ARG_TBL_SIZE 8           /* Size of static argument table. */

/*
 * Flags used during conversion.
 */

#define ALT      0x001      /* alternate form */
#define LADJUST      0x004      /* left adjustment */
#define LONGINT      0x010      /* long integer */
#define LLONGINT   0x020      /* long long integer */
#define SHORTINT   0x040      /* short integer */
#define ZEROPAD      0x080      /* zero (as opposed to blank) pad */
#define FPT      0x100      /* Floating point number */
#define GROUPING   0x200      /* use grouping ("'" flag) */
/* C99 additional size modifiers: */
#define SIZET      0x400      /* size_t */
#define PTRDIFFT   0x800      /* ptrdiff_t */
#define INTMAXT      0x1000      /* intmax_t */
#define CHARINT      0x2000      /* print char using int format */

#define INTMAX_SIZE   (INTMAXT|SIZET|PTRDIFFT|LLONGINT)


/*
 * Choose PADSIZE to trade efficiency vs. size.  If larger printf
 * fields occur frequently, increase PADSIZE and make the initialisers
 * below longer.
 */

#define   PADSIZE   16      /* pad chunk size */
extern char blanks[PADSIZE];
extern char zeroes[PADSIZE];

extern const char xdigs_lower[17];
extern const char xdigs_upper[17];


#endif // ifndef _BSDFMT_H_
