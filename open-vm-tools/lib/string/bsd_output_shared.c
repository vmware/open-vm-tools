/* **********************************************************
 * Copyright 2006 VMware, Inc.  All rights reserved.
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
 * Shared code common to the bsd_output_* implementation files.
 */

//#include <sys/cdefs.h>

#if !defined(STR_NO_WIN32_LIBS) && !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__NetBSD__) && !defined(__ANDROID__)

#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <stddef.h>
#include <stdint.h>
#endif
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <wchar.h>
#ifndef _WIN32
#include <pthread.h>
#endif

#include "vmware.h"
#include "bsd_output_int.h"

#ifndef NO_FLOATING_POINT

/*
 *-----------------------------------------------------------------------------
 *
 * dtoa --
 *
 *    Pretend to be like the mysterious dtoa function in the FreeBSD
 *    libc source code. It appears to take a double argument, and then
 *    return an ASCII character string representation of this number -
 *    just digits, no sign, decimal point, or exponent symbol.
 *
 *    If 'mode' is 3, then 'prec' limits the number of digits after the
 *    decimal point, if 'mode' is 2, then total digits.
 *
 *    The base-10 exponent of the number is returned in 'expOut'.
 *
 *    'sign' is returned as 0 for a positive number, otherwise negative.
 *
 *    'strEnd' is returned as a pointer to the end of the number in the
 *    string, so don't rely on NULL-termination to tell you where the
 *    number ends.
 *
 * Results:
 *
 *    The allocated string on success (free with freedtoa), NULL on
 *    failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

char *
dtoa(double d,       // IN
     int mode,       // IN
     int prec,       // IN
     int *expOut,    // OUT
     int *sign,      // OUT
     char **strEnd)  // OUT
{
   char *str = NULL;
   int dec;

#if defined(_WIN32)
   if (2 == mode) {
      str = malloc(_CVTBUFSIZE);
      if (str) {
         if (_ecvt_s(str, _CVTBUFSIZE, d, prec, &dec, sign)) {
            free(str);
            str = NULL;
         }
      }
   } else {
      ASSERT(3 == mode);
      str = malloc(_CVTBUFSIZE);
      if (str) {
         if (_fcvt_s(str, _CVTBUFSIZE, d, prec, &dec, sign)) {
            free(str);
            str = NULL;
         }
      }

      /*
       * When the value is not zero but rounds to zero at prec digits,
       * the Windows fcvt() sometimes returns the empty string and
       * a negative dec that goes too far (as in -dec > prec).
       * For example, converting 0.001 with prec 1 results in
       * the empty string and dec -2.  (See bug 253674.)
       *
       * We just clamp dec to -prec when this happens.
       *
       * While this may appear to be a safe and good thing to
       * do in general.  It really only works when the result is
       * all zeros or empty.  Since checking for all zeros is
       * expensive, we only check for empty string, which works
       * for this bug.
       */

      if (str && *str == '\0' && dec < 0 && dec < -prec) {
	 dec = -prec;
      }
   }
#else // _WIN32
   static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

   if (2 == mode) {
      pthread_mutex_lock(&mutex);
      str = strdup(ecvt(d, prec, &dec, sign));
      pthread_mutex_unlock(&mutex);
   } else {
      ASSERT(3 == mode);

#ifdef __APPLE__
      /*
       * The Mac fcvt() returns "" when prec is 0, so we have to
       * compensate.  See bug 233530.
       * While it is conceivable that fcvt(round(d), 1) can return
       * a string that doesn't end in 0, it doesn't seem to happen
       * in practice (on the Mac).  The problematic case that we
       * want to avoid is a last digit greater than 4, which requires
       * rounding up, which we don't want to do, which is why we're
       * doing the rounding on the number instead of after fcvt()
       * in the first place.
       * -- edward
       */

      if (prec == 0) {
	 size_t l;
         pthread_mutex_lock(&mutex);
	 str = strdup(fcvt(round(d), 1, &dec, sign));
         pthread_mutex_unlock(&mutex);
	 if (str) {
	    l = strlen(str);
	    ASSERT(l > 0);
	    l--;
	    ASSERT(str[l] == '0');
	    str[l] = '\0';
         }
      } else 
#endif // __APPLE__
      {
         pthread_mutex_lock(&mutex);
         str = strdup(fcvt(d, prec, &dec, sign));
         pthread_mutex_unlock(&mutex);
      }
   }
#endif // _WIN32

   if (str) {
      *strEnd = str + strlen(str);

      /* strip trailing zeroes */
      while ((*strEnd > str) && ('0' == *((*strEnd) - 1))) {
         (*strEnd)--;
      }

      *expOut = dec;
   }

   return str;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ldtoa --
 *
 *    A dtoa wrapper that simply casts its long double argument to a
 *    double. Windows can't handle long double.
 *
 * Results:
 *    See dtoa.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

char *
ldtoa(long double *ld, int mode, int prec, int *exp, int *sign, char **strEnd)
{
   double d = (double) *ld; // ghetto fabulous
   return dtoa(d, mode, prec, exp, sign, strEnd);
}


/*
 *-----------------------------------------------------------------------------
 *
 * freedtoa --
 *
 *    Free the result of dtoa and ldtoa.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
freedtoa(void *mem)
{
   free(mem);
}

#if defined _MSC_VER && _MSC_VER < 1400
/* VC80 has a built-in wmemchar */
/*
 *-----------------------------------------------------------------------------
 *
 * wmemchr --
 *
 *    Stolen from FreeBSD. Find 'c' in 's', which is 'n' characters
 *    long.
 *
 * Results:
 *    The pointer to the first occurence of 'c' in 's', NULL otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

const wchar_t *
wmemchr(const wchar_t *s, wchar_t c, size_t n)
{
   size_t i;

   for (i = 0; i < n; i++) {
      if (*s == c) {
         /* LINTED const castaway */
         return (wchar_t *)s;
      }
      s++;
   }
   return NULL;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * btowc --
 *
 *    Stolen from FreeBSD. Convert the MBCS character 'c' to a wide
 *    character.
 *
 * Results:
 *    The wide character on success, WEOF on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

wint_t
bsd_btowc(int c)
{
   char cc;
   wchar_t wc;

   if (c == EOF)
      return (WEOF);
   /*
    * We expect mbtowc() to return 0 or 1, hence the check for n > 1
    * which detects error return values as well as "impossible" byte
    * counts.
    */
   cc = (char)c;
   if (mbtowc(&wc, &cc, 1) > 1)
      return (WEOF);
   return (wc);
}

#endif /* !NO_FLOATING_POINT */

#endif /* !STR_NO_WIN32_LIBS|*BSD */
