/* **********************************************************
 * Copyright (C) 2007-2016 VMware, Inc.  All rights reserved.
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
 * msgfmt.c --
 *
 *	MsgFmt: format messages for the Msg module
 */


#ifdef VMKERNEL
   #include "vmkernel.h"
   #include "vm_types.h"
   #include "vm_libc.h"
#else
   #include <stdlib.h>
   #include <stdio.h>
   #include <stdarg.h>
   #include <stddef.h>
   #include <string.h>
   #if defined(__FreeBSD__)
      #include <sys/param.h>
   #endif
   #if !defined(_WIN32) && !defined(SOL9) && \
       (!defined(__FreeBSD__) || __FreeBSD_version >= 500029)
      #include <stdint.h>
   #endif
   #if !defined(__FreeBSD__) || __FreeBSD_version >= 400017
      #include <wchar.h>
   #endif

   #include "vmware.h"
   #include "bsdfmt.h"
   #include "err.h"
#endif

#include "msgfmt.h"

#ifdef HAS_BSD_PRINTF
   #include <limits.h>
   #include <locale.h>
   #include "msgid.h"
#endif

/*
 * Older versions of FreeBSD don't have C99 support (stdint), and also
 * do not have wide character support. Re-implement the stuff we need
 * in those cases.
 */

#if defined(__FreeBSD__) && __FreeBSD_version <= 320001
static INLINE const wchar_t *
wmemchr(const wchar_t *s, wchar_t c, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++) {
      if (s[i] == c) {
         return &s[i];
      }
   }

   return NULL;
}

static INLINE size_t
wcslen(const wchar_t *s)
{
   size_t i;

   for (i = 0; s[i]; i++);

   return i;
}
#endif

/*
 * The vmkernel doesn't have the Str module, malloc(), or
 * some of the standard C string functions.
 * The only ones we really need are Str_Vsnprintf() and memchr().
 */

#ifdef VMKERNEL // {

typedef int32 wchar_t;
typedef int32 wint_t;
typedef int64 intmax_t;
typedef size_t ptrdiff_t;

#define STUB(t, f, a) \
	static INLINE t f a { NOT_IMPLEMENTED(); return (t) 0;}
#define VSTUB(f, a) \
	static INLINE void f a { NOT_IMPLEMENTED(); }
STUB(char *, Str_Vasprintf, (char **b, const char *f, va_list a))
STUB(void *, malloc, (size_t s))
STUB(void *, realloc, (void *p, size_t s))
STUB(wchar_t *, wmemchr, (const wchar_t *s, wchar_t c, size_t n))
STUB(size_t, wcslen, (const wchar_t *s))
STUB(char *, strdup, (const char *s))
VSTUB(free, (void *p))
#undef STUB
#undef VSTUB

typedef int Err_Number;
#define ERR_INVALID (-1)
static INLINE Err_Number
Err_String2Errno(const char *string)
{
   return ERR_INVALID;
}

#ifdef VMX86_DEBUG
static INLINE Err_Number
Err_String2ErrnoDebug(const char *string)
{
   return ERR_INVALID;
}
#endif

static INLINE int
Str_Vsnprintf(char *str, size_t size, const char *format, va_list ap) {
   int n = vsnprintf(str, size, format, ap);
   ASSERT(n >= 0);
   if (n >= size) {
      str[size - 1] = '\0';
      n = -1;
   }
   return n;
}

static INLINE const void *
memchr(const void *s, int c, size_t n)
{
   const uint8 *p = s;
   const uint8 *e = p + n;
   while (p < e) {
      if (*p++ == c) {
	 return p;
      }
   }
   return NULL;
}

#endif // }

#if defined __ANDROID__
/*
 * Android doesn't support dtoa().
 */
#define NO_DTOA
#endif


/*
 * Local data
 */

typedef struct MsgFmtParseState {
   MsgFmt_Arg *args;
   int numArgs;
   int maxArgs;
   char *error;

   /*
    * Allocator state for caller-supplied buffer.
    */

   void *buf;
   char *bufp;
   char *bufe;
} MsgFmtParseState;

/* d, i, o, u, x, X, e, E, f, F, g, G, a, A, c, s, C, S, p, and n --hpreg */
static int const isSpecifier[] = {
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
   0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1, 1,
   1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};


/*
 * Local functions
 */

static MsgFmt_SpecFunc MsgFmtGetArg1;
static int MsgFmtAToI(char const **start, char const *end);
static void MsgFmtError(MsgFmtParseState *state, const char *fmt, ...);

static void MsgFmtAllocInit(MsgFmtParseState *state, void *buf, size_t size);
static void *MsgFmtAlloc(MsgFmtParseState *state, size_t size);
static Bool MsgFmtAllocArgs(MsgFmtParseState *state, int n);
static char *MsgFmtVasprintf(MsgFmtParseState *state,
                             const char *fmt, va_list args);
static void MsgFmtFreeAll(MsgFmtParseState *state);
static size_t MsgFmtBufUsed(MsgFmtParseState *state);
#ifdef HAS_BSD_PRINTF
static int MsgFmtSnprintfWork(char **outbuf, size_t bufSize, const char *fmt0,
                              const struct MsgFmt_Arg *args, int numArgs);
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmt_ParseWin32 --
 *
 *    Convert the Win32 representation of a format string into another
 *    representation --hpreg
 *
 *    XXX I haven't implemented %0 and %n, because they suck:
 *        . they mix content and presentation
 *        . they have nothing to do with parameters and hence have no
 *          equivalent in other systems
 *
 * Results:
 *     0 on success
 *    -1 on failure: out of memory
 *    -2 on failure: invalid 'in'
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
MsgFmt_ParseWin32(MsgFmt_LitFunc *litFunc,    // IN
                  MsgFmt_SpecFunc *specFunc,  // IN
                  void *clientData,           // IN
                  char const *in)             // IN
{
   char const *startUnescaped;
   unsigned int sm;
   char const *pos = 0 /* Compiler warning --hpreg */;
   char const *type = 0 /* Compiler warning --hpreg */;
   int status;

   startUnescaped = in;
   sm = 0;

   for (; *in != '\0'; in++) {
      /* Unsigned does matter --hpreg */
      unsigned char ubyte;

      ubyte = *in;
      switch (sm) {
      case 2: /* Found %<1-9>...<byte> --hpreg */
         if (ubyte >= '0' && ubyte <= '9') {
            break;
         }
         if (ubyte == '!') {
            type = in + 1;
	    sm = 3;
            break;
         }
         if ((status = (*litFunc)(clientData, startUnescaped,
                                  pos - 1 - startUnescaped)) < 0 ||
             (status = (*specFunc)(clientData, pos, in - pos, "s", 1)) < 0) {
            return status;
         }
         startUnescaped = in;
         sm = 0;
         /* Fall through --hpreg */

      case 0: /* Found <byte> --hpreg */
         if (ubyte == '%') {
            pos = in + 1;
            sm = 1;
         }
         break;

      case 1: /* Found %<byte> --hpreg */
         if (ubyte >= '1' && ubyte <= '9') {
            sm = 2;
         } else {
            VERIFY(ubyte != '0' && ubyte != 'n');
            status = (*litFunc)(clientData, startUnescaped,
                                in - 1 - startUnescaped);
            if (status < 0) {
               return status;
            }
            startUnescaped = in;
            sm = 0;
         }
         break;

      case 3: /* Found %<1-9>...!...<byte> --hpreg */
         if (ubyte == '!') {
            if (   (status = (*litFunc)(clientData, startUnescaped,
                                        pos - 1 - startUnescaped)) < 0
                || (status = (*specFunc)(clientData, pos, type - 1 - pos,
                		         type, in - type)) < 0) {
               return status;
            }
            startUnescaped = in + 1;
            sm = 0;
         }
         break;

      default:
         NOT_IMPLEMENTED();
         break;
      }
   }

   switch (sm) {
   case 0:
      status = (*litFunc)(clientData, startUnescaped, in - startUnescaped);
      if (status < 0) {
         return status;
      }
      break;

   case 2:
      if (   (status = (*litFunc)(clientData, startUnescaped,
                                  pos - 1 - startUnescaped)) < 0
          || (status = (*specFunc)(clientData, pos, in - pos, "s", 1)) < 0) {
         return status;
      }
      break;

   case 1:
   case 3:
      return -2;
      break;

   default:
      NOT_IMPLEMENTED();
      break;
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmt_Parse --
 *
 *    Parse a message format.
 *
 * Results:
 *     0 on success
 *    -1 on failure: out of memory
 *    -2 on failure: invalid 'in'
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
MsgFmt_Parse(MsgFmt_LitFunc *litFunc,    // IN
             MsgFmt_SpecFunc *specFunc,  // IN
             void *clientData,           // IN
             char const *in)             // IN
{
   char const *startUnescaped;
   unsigned int sm;
   unsigned int counter;
   int status;
   char const *startEscaped = 0 /* Compiler warning --hpreg */;
   char const *type = 0 /* Compiler warning --hpreg */;
   Bool usePos = FALSE /* Compiler warning --hpreg */;

   startUnescaped = in;
   sm = 0;
   counter = 0;

   for (; *in != '\0'; in++) {
      /* Unsigned does matter --hpreg */
      unsigned char ubyte;

      ubyte = *in;
      switch (sm) {
      case 0: /* Found <byte> --hpreg */
         if (ubyte == '%') {
            sm = 1;
         }
         break;

      case 1: /* Found %<byte> --hpreg */
         if (ubyte == '%') {
	    if (litFunc != NULL &&
	        (status = (*litFunc)(clientData, startUnescaped,
				     in - 1 - startUnescaped)) < 0) {
	       return status;
	    }
            startUnescaped = in;
            sm = 0;
            break;
         }
         startEscaped = in;
         type = in;
         if (ubyte >= '1' && ubyte <= '9') {
            sm = 2;
            break;
         }
         sm = 3;
         /* Fall through --hpreg */

      case 3: /* Found %<1-9>...$...<byte> or %...<byte> --hpreg */
      variant3:
         if (isSpecifier[ubyte]) {
            char const *pos;
            char const *posEnd;
            char posBuf[10 /* 32 bits unsigned in decimal --hpreg */];

            if (counter) {
               if (usePos != (startEscaped != type)) {
                  return -2;
               }
            } else {
               usePos = (startEscaped != type);
            }
            counter++;

            if (usePos) {
               pos = startEscaped;
               posEnd = type - 1;
            } else {
               char *current;
               unsigned int value;

               current = posBuf + sizeof(posBuf);
               posEnd = current;
               value = counter;
               ASSERT(value);
               do {
                  current--;
                  ASSERT(current >= posBuf);
                  *current = '0' + value % 10;
                  value /= 10;
               } while (value);
               pos = current;
            }

            if (litFunc != NULL &&
		(status = (*litFunc)(clientData, startUnescaped,
				     startEscaped - 1 - startUnescaped)) < 0) {
	       return status;
	    }
            if ((status = (*specFunc)(clientData, pos, posEnd - pos, type,
                                      in + 1 - type)) < 0) {
               return status;
            }
            startUnescaped = in + 1;
            sm = 0;
            break;
         }
         /* Digits for field width & precision, zero for leading zeroes,
	    and dot for separator between width and precision. */
         if ((ubyte >= '0' && ubyte <= '9') || ubyte == '.') {
            break;
         }
	 /* Flags */
         if (ubyte == '#' || ubyte == '-' || ubyte == ' ' || ubyte == '+' ||
             ubyte == '\'') {
	    break;
	 }
	 /* Length modifiers */
	 if (ubyte == 'L' || ubyte == 'l' || ubyte == 'h' || ubyte == 'z' ||
	     ubyte == 'Z' || ubyte == 't' || ubyte == 'q' || ubyte == 'j' ||
	     ubyte == 'I') {
            break;
         }
         return -2;

      case 2: /* Found %<1-9>...<byte> --hpreg */
         if (ubyte >= '0' && ubyte <= '9') {
            break;
         }
         if (ubyte == '$') {
            type = in + 1;
            sm = 3;
            break;
         }
         sm = 3;
         goto variant3;

      default:
         NOT_IMPLEMENTED();
         break;
      }
   }

   if (sm) {
      return -2;
   }
   if (litFunc != NULL &&
       (status = (*litFunc)(clientData, startUnescaped,
			    in - startUnescaped)) < 0) {
      return status;
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmt_ParseSpec --
 *
 *      Given a format specifier (the % stuff), return its contituent parts.
 *
 * Results:
 *      0 on success, -2 (bad format) on failure.
 *	Out parameters:
 *	   Width and precision are -1 if not specified.
 *	   Length modifier is '\0' if not specified.
 *	   Length modifier of "ll", "I64", or "q" is returned as 'L'.
 *	   (This means we freely allow %llf and %qf, which is not strictly
 *	   correct.  However, glibc printf allows them (as well as %Ld),
 *	   and they mean the same thing.)
 *	   Length modifier of "hh" is returned as 'H'.
 *	   Length modifier of "Z" is returned as 'z', for compatibility
 *	   with old glibc.
 *	On failure, some or all of the out parameters may be modified
 *	in an undefined manner.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
MsgFmt_ParseSpec(char const *pos,       // IN: n$ location
                 unsigned int posSize,  // IN: n$ length
                 char const *type,      // IN: specifier after position
                 unsigned int typeSize, // IN: size of above
		 int *position,         // OUT: argument position
		 int *flags,            // OUT: flags
		 int *width,            // OUT: width
		 int *precision,        // OUT: precision
		 char *lengthMod,       // OUT: length modifier
		 char *conversion)      // OUT: conversion specifier
{
   char const *p = type;
   char const *end = type + typeSize;

   /*
    * Convert argument position to int.
    * Fail if not a good decimal number greater than 0.
    */

   {
      char const *posEnd = pos + posSize;
      *position = MsgFmtAToI(&pos, posEnd);
      if (*position <= 0 || pos != posEnd) {
	 return -2;
      }
   }

   /*
    * The format specifier is, in this order,
    *    zero or more flags
    *    an optional width (a decimal number or *)
    *    an optional precision (. followed by optional decimal number or *)
    *    an optional length modifier (l, L, ll, z, etc.)
    *    conversion specifier (a character)
    *
    * The rest of this module does not recognize * as width or precision,
    * so we don't do it here either.
    *
    * glibc 2.2 supports the I flag, which we don't.  Instead, we
    * support the I, I32, and I64 length modifiers used by Microsoft.
    */

   /*
    * Flags
    */

   *flags = 0;
   for (; p < end; p++) {
      switch (*p) {
      case '#':
	 *flags |= MSGFMT_FLAG_ALT;
	 continue;
      case '0':
	 *flags |= MSGFMT_FLAG_ZERO;
	 continue;
      case '-':
	 *flags |= MSGFMT_FLAG_MINUS;
	 continue;
      case ' ':
	 *flags |= MSGFMT_FLAG_SPACE;
	 continue;
      case '+':
	 *flags |= MSGFMT_FLAG_PLUS;
	 continue;
      case '\'':
	 *flags |= MSGFMT_FLAG_QUOTE;
	 continue;

      default:
	 break;
      }
      break;
   }

   /*
    * Width
    */

   if (p >= end || *p < '1' || *p > '9') {
      *width = -1;
   } else {
      *width = MsgFmtAToI(&p, end);
      if (*width < 0) {
	 return -2;
      }
   }

   /*
    * Precision
    */

   if (p >= end || *p != '.') {
      *precision = -1;
   } else {
      p++;
      *precision = MsgFmtAToI(&p, end);
      if (*precision < 0) {
	 return -2;
      }
   }

   /*
    * Length modifier
    */

   if (p >= end) {
      return -2;
   }
   *lengthMod = '\0';
   switch (*p) {
   case 'h':
      p++;
      if (p >= end || *p != 'h') {
	 *lengthMod = 'h';
      } else {
	 p++;
	 *lengthMod = 'H';
      }
      break;
   case 'l':
      p++;
      if (p >= end || *p != 'l') {
	 *lengthMod = 'l';
      } else {
	 p++;
	 *lengthMod = 'L';
      }
      break;
   case 'I':
      /*
       * Microsoft:
       *    I64 is 64-bit number.  For us, the same as L.
       *    I32 is 32-bit number.  For us, nothing.
       *    I is size_t.
       */
      if (p + 2 < end && p[1] == '6' && p[2] == '4') {
	 p += 3;
	 *lengthMod = 'L';
      } else if (p + 2 < end && p[1] == '3' && p[2] == '2') {
	 p += 3;
      } else {
	 p++;
	 *lengthMod = 'z';
      }
      break;
   case 'q':
      p++;
      *lengthMod = 'L';
      break;
   case 'Z':
      p++;
      *lengthMod = 'z';
      break;
   case 'L':
   case 'j':
   case 'z':
   case 't':
      *lengthMod = *p++;
      break;
   }

   /*
    * Conversion specifier
    *
    * Return false if no conversion specifier or not the last character.
    */

   if (p + 1 == end && isSpecifier[(unsigned char) *p]) {
      *conversion = *p;
      return 0;
   }
   return -2;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmtAToI --
 *
 *      Convert numeric string to integer.
 *	The range is 0 to MAX_INT32 (nonnegative 32-bit signed int).
 *	Empty string or a string that does not begin with
 *	a digit is treated as 0.
 *
 * Results:
 *      The number or -1 on overflow.
 *	Start pointer updated to point to first nonnumeric character.
 *	or first character before overflow.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
MsgFmtAToI(char const **start,	// IN/OUT: string pointer
           char const *end)	// IN: end of string
{
   char const *p;
   int n = 0;

   ASSERT_ON_COMPILE(sizeof (int) >= 4);
   for (p = *start; p < end && *p >= '0' && *p <= '9'; p++) {
      if (n > MAX_INT32 / 10) {
         n = -1;
         break;
      }
      n *= 10;
      n += *p - '0';
      if (n < 0) {
         n = -1;
         break;
      }
   }
   *start = p;
   return n;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmt_GetArgs --
 *
 *      Parse a format string and return the arguments implied by it.
 *
 * Results:
 *	TRUE on sucess.
 *	Out parameters:
 *        The array of MsgFmt_Arg structures.
 *	  The number of arguments.
 *	  An error string on failure.
 *
 * Side effects:
 *      Memory is allocated.
 *
 *-----------------------------------------------------------------------------
 */

Bool
MsgFmt_GetArgs(const char *fmt,		// IN: format string
	       va_list va,		// IN: the argument list
	       MsgFmt_Arg **args,	// OUT: the returned arguments
	       int *numArgs,		// OUT: number of returned arguments
	       char **error)		// OUT: error string
{
   return MsgFmt_GetArgsWithBuf(fmt, va, args, numArgs, error, NULL, NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmt_GetArgsWithBuf --
 *
 *      Parse a format string and return the arguments implied by it.
 *
 *	If buf is supplied, allocate memory there instead of with malloc().
 *
 * Results:
 *	TRUE on sucess.
 *	Out parameters:
 *        The array of MsgFmt_Arg structures.
 *	  The number of arguments.
 *	  An error string on failure.
 *	  The amount of buf used (if caller supplied buf)
 *
 * Side effects:
 *      Memory may be allocated.
 *
 *-----------------------------------------------------------------------------
 */

Bool
MsgFmt_GetArgsWithBuf(const char *fmt,	  // IN: format string
	              va_list va,	  // IN: the argument list
	              MsgFmt_Arg **args,  // OUT: the returned arguments
	              int *numArgs,	  // OUT: number of returned arguments
	              char **error,	  // OUT: error string
                      void *buf,	  // OUT: memory to store output
	              size_t *bufSize)	  // IN/OUT: size of buf /
                                          //         amount of buf used
{
   MsgFmtParseState state;
   int status;
   int i;

   memset(&state, 0, sizeof state);
   if (buf != NULL) {
      ASSERT(bufSize != NULL);
      MsgFmtAllocInit(&state, buf, *bufSize);
   }

   /*
    * First pass: parse format to get argument information
    */

   status = MsgFmt_Parse(NULL, MsgFmtGetArg1, &state, fmt);
   if (status < 0) {
      goto bad;
   }

   /*
    * Second pass: get argument values
    *
    * While we can store most values directly in the MsgFmt_Arg
    * structure, strings have to be copied into allocated space.
    * When precision is specified (see comment about it in
    * MsgFmtGetArg1()), we copy at most that many bytes because
    * that's how many printf() looks at, and we must not touch
    * memory beyond what printf() would.
    */

   for (i = 0; i < state.numArgs; i++) {
      MsgFmt_Arg *a = state.args + i;
      switch (a->type) {
      case MSGFMT_ARG_INVALID:
	 MsgFmtError(&state, "MsgFmt_GetArgs: gap in arguments at position %d",
		     i + 1);
	 goto bad;
	 break;

      case MSGFMT_ARG_INT32:
	 ASSERT_ON_COMPILE(sizeof (int) == sizeof (int32));
	 a->v.signed32 = va_arg(va, int);
	 break;
      case MSGFMT_ARG_INT64:
	 ASSERT_ON_COMPILE(sizeof (long long) == sizeof (int64));
	 a->v.signed64 = va_arg(va, long long);
	 break;

      case MSGFMT_ARG_PTR32:
	 // we can only handle this case if native pointer is 4 bytes
	 ASSERT(sizeof (void *) == sizeof (uint32));
	 a->v.unsigned32 = (uint32) (uintptr_t) va_arg(va, void *);
	 break;
      case MSGFMT_ARG_PTR64:
	 // we can only handle this case if native pointer is 8 bytes
	 ASSERT(sizeof (void *) == sizeof (uint64));
	 a->v.unsigned64 = (uint64) (uintptr_t) va_arg(va, void *);
	 break;

#ifndef NO_FLOATING_POINT
      case MSGFMT_ARG_FLOAT64:
         ASSERT_ON_COMPILE(sizeof (double) == 8);
         a->v.float64 = va_arg(va, double);
	 break;
#endif

      case MSGFMT_ARG_STRING8: {
	 const char *p = va_arg(va, char *);
	 size_t n;
	 Err_Number errorNumber;
	 ASSERT_ON_COMPILE(sizeof (char) == sizeof (int8));
	 ASSERT_ON_COMPILE(offsetof(MsgFmt_Arg, v.string8) ==
		           offsetof(MsgFmt_Arg, v.ptr));
	 if (p == NULL) {
	    a->v.string8 = NULL;
	 } else {
	    if (a->p.precision < 0) {
	       n = strlen(p);
	    } else {
	       const char *q;
	       n = a->p.precision;
	       q = memchr(p, '\0', n);
	       if (q != NULL) {
		  n = q - p;
	       }
	    }
	    // yes, sizeof (int8) is 1.
	    a->v.string8 = MsgFmtAlloc(&state, n + 1);
	    if (a->v.string8 == NULL) {
	       status = -1;
	       goto bad;
	    }
	    memcpy(a->v.string8, p, n);
	    a->v.string8[n] = '\0';
	 }
	 errorNumber = Err_String2Errno(p);
#ifdef VMX86_DEBUG
	 if (errorNumber == ERR_INVALID && p != NULL) {
	    // p may not be null terminated, so use string8
	    errorNumber = Err_String2ErrnoDebug(a->v.string8char);
	    if (errorNumber != ERR_INVALID) {
	       // Err_String2ErrnoDebug already logged its info
	       Log("%s: failed to look up copied error string at %p.\n",
		   __FUNCTION__, p);
	    }
	 }
#endif
	 if (errorNumber != ERR_INVALID &&
	     MSGFMT_CURRENT_PLATFORM != MSGFMT_PLATFORM_UNKNOWN) {
	    ASSERT_ON_COMPILE(sizeof errorNumber == sizeof a->e.number);
	    a->type = MSGFMT_ARG_ERRNO;
	    a->e.platform = MSGFMT_CURRENT_PLATFORM;
	    a->e.number = errorNumber;
	    break;
	 }
	 break;
	 }
      case MSGFMT_ARG_STRING16:
      case MSGFMT_ARG_STRING32: {
	 // we can only handle the case when native wchar_t matches
	 // the string char size
	 const wchar_t *p = va_arg(va, wchar_t *);
	 size_t n;
	 ASSERT(a->type == MSGFMT_ARG_STRING16 ?
	        sizeof (wchar_t) == sizeof (int16) :
	        sizeof (wchar_t) == sizeof (int32));
	 ASSERT_ON_COMPILE(offsetof(MsgFmt_Arg, v.string16) ==
		           offsetof(MsgFmt_Arg, v.ptr));
	 ASSERT_ON_COMPILE(offsetof(MsgFmt_Arg, v.string32) ==
		           offsetof(MsgFmt_Arg, v.ptr));
	 if (p == NULL) {
	    a->v.ptr = NULL;
	 } else {
	    if (a->p.precision < 0) {
	       n = wcslen(p);
	    } else {
	       const wchar_t *q;
	       n = a->p.precision;
	       q = wmemchr(p, 0, n);
	       if (q != NULL) {
	          n = q - p;
	       }
	    }
	    a->v.ptr = MsgFmtAlloc(&state, sizeof (wchar_t) * (n + 1));
	    if (a->v.ptr == NULL) {
	       status = -1;
	       goto bad;
	    }
	    memcpy(a->v.ptr, p, sizeof (wchar_t) * n);
	    ((wchar_t *) a->v.ptr)[n] = 0;
	 }
	 break;
	 }

      case MSGFMT_ARG_ERRNO:	// there shouldn't be this case here
      default:
	 NOT_REACHED();
      }

      // clear private data
      memset(&a->p, 0, sizeof a->p);
   }

   /*
    * Pass results back
    */

   if (args == NULL) {
      MsgFmtFreeAll(&state);
   } else {
      *args = state.args;
   }
   if (numArgs != NULL) {
      *numArgs = state.numArgs;
   }
   if (bufSize != NULL) {
      *bufSize = MsgFmtBufUsed(&state);
   }
   ASSERT(state.error == NULL);
   *error = NULL;
   return TRUE;

bad:
   if (state.error == NULL) {
      switch (status) {
      case -1:
	 MsgFmtError(&state, "MsgFmt_GetArgs: out of memory");
	 break;
      case -2:
	 MsgFmtError(&state, "MsgFmt_GetArgs: error in format string");
	 break;
      default:
	 MsgFmtError(&state, "MsgFmt_GetArgs: error %d", status);
      }
   }
   ASSERT(state.args == NULL);	// MsgFmtError() frees args
   *error = state.error;
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmtGetArg1 --
 *
 *      Process one format specifier for MsgFmt_GetArgs().
 *	Called by MsgFmt_Parse().
 *
 * Results:
 *      0 on success,
 *	negative status on failure (see MsgFmt_Parse()).
 *	error string in state.error on failure.
 *
 * Side effects:
 *      Memory is allocated.
 *
 *-----------------------------------------------------------------------------
 */

static int
MsgFmtGetArg1(void *clientData,      // IN: state
              const char *pos,	     // IN: n$ location
              unsigned int posSize,  // IN: n$ length
              char const *type,      // IN: specifier after position
              unsigned int typeSize) // IN: size of above
{
   MsgFmtParseState *state = clientData;
   MsgFmt_Arg *a;
   int position;
   int flags;
   int width;
   int precision;
   char lengthMod;
   char conversion;
   MsgFmt_ArgType argType = MSGFMT_ARG_INVALID;
   int status;

   /*
    * Parse format specifier
    */

   status = MsgFmt_ParseSpec(pos, posSize, type, typeSize,
			     &position, &flags, &width, &precision,
			     &lengthMod, &conversion);
   if (status < 0) {
      MsgFmtError(state,
		  "MsgFmtGetArg1: bad specifier, "
	          "status %d, pos \"%.*s\", type \"%.*s\"",
	          status, posSize, pos, typeSize, type);
      return status;
   }

   /*
    * Make room in argument array if necessary.
    */

   if (position > state->numArgs) {
      if (!MsgFmtAllocArgs(state, position)) {
	 MsgFmtError(state, "MsgFmtGetArg1: out of memory at arg %d",
		     position);
	 return -1;
      }
      state->numArgs = position;
   }

   /*
    * Fill in argument structure based on the format specifier.
    *
    * For strings, the precision argument is the maximum length
    * to print.  We need to keep track of it so MsgFmt_GetArgs()
    * can know how many characters to squirrel away, in case
    * the string isn't null terminated, is very long, or falls off
    * the end of the world.
    *
    * In all other cases, the precision is unimportant to us
    * and we don't keep it around.
    */

   a = state->args + position - 1;

   switch (conversion) {
   case 'd':
   case 'i':
   case 'o':
   case 'u':
   case 'x':
   case 'X':
      switch (lengthMod) {
      // all of these take an int argument, they just print differently
      case '\0':
      case 'h':
      case 'H':
	 ASSERT_ON_COMPILE(sizeof (int) == sizeof (int32));
	 argType = MSGFMT_ARG_INT32;
	 break;

      case 'l':
	 ASSERT_ON_COMPILE(sizeof (long) == sizeof (int32) ||
			   sizeof (long) == sizeof (int64));
	 if (sizeof (long) == sizeof (int32)) {
	    argType = MSGFMT_ARG_INT32;
	 } else {
	    argType = MSGFMT_ARG_INT64;
	 }
	 break;

      case 'j':
#ifndef _WIN32 // no intmax_t, bsd_vsnprintf() uses 64 bits
	 ASSERT_ON_COMPILE(sizeof (intmax_t) == sizeof (int64));
#endif
      case 'L':
	 ASSERT_ON_COMPILE(sizeof (long long) == sizeof (int64));
	 argType = MSGFMT_ARG_INT64;
	 break;

      case 't':
	 ASSERT_ON_COMPILE(sizeof (ptrdiff_t) == sizeof (size_t));
      case 'z':
	 ASSERT_ON_COMPILE(sizeof (size_t) == sizeof (int32) ||
		           sizeof (size_t) == sizeof (int64));
	 if (sizeof (size_t) == sizeof (int32)) {
	    argType = MSGFMT_ARG_INT32;
	 } else {
	    argType = MSGFMT_ARG_INT64;
	 }
	 break;
      default:
	 NOT_REACHED();
      }
      break;

   case 'e':
   case 'E':
   case 'f':
   case 'F':
   case 'g':
   case 'G':
   case 'a':
   case 'A':
#ifndef NO_FLOATING_POINT
      switch (lengthMod) {
      // l h hh t z are not defined by man page, but allowed by glibc
      case '\0':
      case 'l':
      case 'h':
      case 'H':
      case 't':
      case 'z':
	 ASSERT_ON_COMPILE(sizeof (double) == 8);
	 argType = MSGFMT_ARG_FLOAT64;
	 break;
      // j is not defined by man page, but allowed by glibc
      case 'L':
      case 'j':
	 /*
	  * We don't do %Lf because it's not that useful to us, and
	  * long double has a number of implementations.  For example,
	  * on Win32 it's the same as double, and it would have a hard
	  * time dealing with a bigger one passed to it.
	  * We can just coerce it down to a double at the source,
	  * but then why bother?
	  */
	 MsgFmtError(state,
		     "MsgFmtGetArg1: %%%c%c not supported, "
	             "pos \"%.*s\", type \"%.*s\"",
	             lengthMod, conversion, posSize, pos, typeSize, type);
	 return -2;
      default:
	 NOT_REACHED();
      }
      break;
#else
      MsgFmtError(state,
                  "MsgFmtGetArg1: %%%c%c not supported, "
                  "pos \"%.*s\", type \"%.*s\"",
                  lengthMod, conversion, posSize, pos, typeSize, type);
      return -2;
#endif /*! NO_FLOATING_POINT */

   case 'c':
      switch (lengthMod) {
      // h hh t z not defined by man page, but allowed by glibc
      case '\0':
      case 'h':
      case 'H':
      case 't':
      case 'z':
	 ASSERT_ON_COMPILE(sizeof (int) == sizeof (int32));
	 argType = MSGFMT_ARG_INT32;
	 break;
      // j ll L not defined by man page nor actually supported
      case 'l':
      case 'j':
      case 'L':
	 goto caseC;
      default:
	 NOT_REACHED();
      }
      break;

   case 'C':
   caseC:
      // man page says it's a wint_t argument, but we assume promotion to int
      ASSERT_ON_COMPILE(sizeof (wint_t) <= sizeof (int) &&
	                sizeof (int) == sizeof (int32));
      argType = MSGFMT_ARG_INT32;
      break;

   case 's':
      // we interpret the length modifier like we do for %c
      switch (lengthMod) {
      case '\0':
      case 'h':
      case 'H':
      case 't':
      case 'z':
	 ASSERT_ON_COMPILE(sizeof (char) == sizeof (int8));
	 argType = MSGFMT_ARG_STRING8;
	 break;
      case 'l':
      case 'j':
      case 'L':
	 goto caseS;
      default:
	 NOT_REACHED();
      }
      // keep track of maximum string length, see block comment above
      a->p.precision = precision;
      ASSERT(a->v.ptr == NULL);
      break;

   case 'S':
   caseS:

#if defined __ANDROID__
      ASSERT_ON_COMPILE(sizeof (wchar_t) == sizeof (int16) ||
	                sizeof (wchar_t) == sizeof (int32) ||
                        sizeof (wchar_t) == sizeof (int8));
#else
      ASSERT_ON_COMPILE(sizeof (wchar_t) == sizeof (int16) ||
                        sizeof (wchar_t) == sizeof (int32));
#endif

      if (sizeof (wchar_t) == sizeof (int16)) {
	 argType = MSGFMT_ARG_STRING16;
#if defined __ANDROID__
      } else if (sizeof (wchar_t) == sizeof (int8)) {
         argType = MSGFMT_ARG_STRING8;
#endif
      } else {
	 argType = MSGFMT_ARG_STRING32;
      }
      // keep track of maximum string length, see block comment above
      a->p.precision = precision;
      ASSERT(a->v.ptr == NULL);
      break;

   case 'p':
      ASSERT_ON_COMPILE(sizeof (void *) == sizeof (int32) ||
	                sizeof (void *) == sizeof (int64));
      if (sizeof (void *) == sizeof (int32)) {
	 argType = MSGFMT_ARG_PTR32;
      } else {
	 argType = MSGFMT_ARG_PTR64;
      }
      break;

   case 'n':
      MsgFmtError(state,
		  "MsgFmtGetArg1: %%n not supported, "
		  "pos \"%.*s\", type \"%.*s\"",
		  posSize, pos, typeSize, type);
      return -2;

   // MsgFmt_ParseSpec() doesn't do %m, and we don't see %%
   default:
      MsgFmtError(state,
		  "MsgFmtGetArg1: %%%c not understood, "
		  "pos \"%.*s\", type \"%.*s\"",
		  conversion, posSize, pos, typeSize, type);
      NOT_REACHED();
   }

   ASSERT(argType != MSGFMT_ARG_INVALID);
   if (a->type != MSGFMT_ARG_INVALID && a->type != argType) {
      MsgFmtError(state,
		  "MsgFmtGetArg1: incompatible specifiers for argument %d, "
	          "old type %d, new type %d, pos \"%.*s\", type \"%.*s\"",
	          position, a->type, argType, posSize, pos, typeSize, type);
      return -2;
   }
   a->type = argType;

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmtError --
 *
 *      Format an error string and squirrel it away.
 *
 * Results:
 *      Error string returned in state variable.
 *
 * Side effects:
 *      Memory may be allocated.
 *
 *-----------------------------------------------------------------------------
 */

static void
MsgFmtError(MsgFmtParseState *state,	// IN/OUT: state structure
            const char *fmt,		// IN: error format
	    ...)			// IN: error args
{
   va_list args;

   ASSERT(state->error == NULL);
   // free up space (in call-supplied buffer) for error string
   MsgFmtFreeAll(state);
   va_start(args, fmt);
   state->error = MsgFmtVasprintf(state, fmt, args);
   va_end(args);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmt_FreeArgs --
 *
 *      Free an array of MsgFmt_Arg structures.
 *	Do not call this on an array in a caller-supplied
 *	buffer from MsgFmt_GetArgsWithBuf().
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Memory is freed.
 *
 *-----------------------------------------------------------------------------
 */

void
MsgFmt_FreeArgs(MsgFmt_Arg *args,	// IN/OUT: arguments to free
                int numArgs)		// IN: number of arguments
{
   int i;

   for (i = 0; i < numArgs; i++) {
      switch (args[i].type) {
      case MSGFMT_ARG_STRING8:
      case MSGFMT_ARG_STRING16:
      case MSGFMT_ARG_STRING32:
      case MSGFMT_ARG_ERRNO:
	 free(args[i].v.ptr);
	 break;
      default:
	 ;
      }
   }
   free(args);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmtAllocInit --
 *
 *	Initialize allocator for caller-supplied buffer.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	As described.
 *
 *-----------------------------------------------------------------------------
 */

static void
MsgFmtAllocInit(MsgFmtParseState *state, // IN/OUT: state structure
	        void *buf,		 // IN: buffer
                size_t size)		 // IN: size to allocate
{
   state->bufp = state->buf = buf;
   state->bufe = state->bufp + size;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmtAlloc --
 *
 *	Allocate memory from malloc() or from supplied buffer.
 *
 * Results:
 *	Pointer or NULL on failure.
 *
 * Side effects:
 *	Memory allocated or state updated.
 *
 *-----------------------------------------------------------------------------
 */

static void *
MsgFmtAlloc(MsgFmtParseState *state,	// IN/OUT: state structure
            size_t size)		// IN: size to allocate
{
   void *p;

   if (state->buf == NULL) {
      p = malloc(size);
   } else {
      if (state->bufe - state->bufp < size) {
	 return NULL;
      }
      p = state->bufp;
      state->bufp += size;
   }
   return p;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmtAllocArgs --
 *
 *	Grow MsgFmt_Arg array to accomodate new entry.
 *
 * Results:
 *	TRUE on success.
 *	State updated.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
MsgFmtAllocArgs(MsgFmtParseState *state, // IN/OUT: state structure
                int n)			 // IN: 1-based argument number
{
   if (n <= state->maxArgs) {
      return TRUE;
   }

   /*
    * If using malloc, then reallocate() the array with some slack.
    * If using our own buffer, just grow it exactly.
    */

   if (state->buf == NULL) {
      void *p;
      n = MAX(4, n + state->maxArgs);
      p = realloc(state->args, n * sizeof *state->args);
      if (p == NULL) {
	 return FALSE;
      }
      state->args = p;
   } else {
      if (state->args == NULL) {
	 // first time
	 state->args = (void *) state->bufp;
      } else {
	 // growing: there must be nothing after the args array
	 ASSERT((void *) state->bufp == state->args + state->maxArgs);
      }
      if ((char *) (state->args + n) > state->bufe) {
	 return FALSE;
      }
      state->bufp = (char *) (state->args + n);
   }
   memset(state->args + state->maxArgs, 0,
	  sizeof *state->args * (n - state->maxArgs));
   state->maxArgs = n;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmtVasprintf --
 *
 *      Format a string in allocated space.
 *
 * Results:
 *      String.
 *
 * Side effects:
 *	Memory allocated or state updated.
 *	Panic if can't allocate.
 *
 *-----------------------------------------------------------------------------
 */

static char *
MsgFmtVasprintf(MsgFmtParseState *state,	// IN/OUT: state structure
                const char *fmt,		// IN: error format
	        va_list args)		// IN: error args
{
   char *p;

   ASSERT(state->error == NULL);
   if (state->buf == NULL) {
      p = Str_Vasprintf(NULL, fmt, args);
      VERIFY(p != NULL);
   } else {
      int n;
      p = state->bufp;
      // Str_Vsnprintf() may truncate
      n = Str_Vsnprintf(p, (char *)state->bufe - p, fmt, args);
      state->bufp = (n < 0) ? state->bufe : state->bufp + n + 1;
   }
   return p;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmtFreeAll --
 *
 *	Free all memory associated with current MsgFmt_Arg array.
 *
 * Results:
 *	State updated.
 *
 * Side effects:
 *	Memory may be freed.
 *
 *-----------------------------------------------------------------------------
 */

static void
MsgFmtFreeAll(MsgFmtParseState *state) // IN/OUT: state structure
{
   if (state->args == NULL)
      return;

   if (state->buf == NULL) {
      MsgFmt_FreeArgs(state->args, state->numArgs);
   } else {
      state->bufp = state->buf;
   }
   state->numArgs = state->maxArgs = 0;
   state->args = NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmtBufUsed --
 *
 *      Return the amount of space used in the caller supplied buffer.
 *
 * Results:
 *      size_t	
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static size_t
MsgFmtBufUsed(MsgFmtParseState *state) // IN: state structure
{
   if (state->buf == NULL) {
      return 0;
   } else {
      return state->bufp - (char *)state->buf;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmt_SwizzleArgs --
 *
 *      Pointer swizzling. Flattens pointers in the MsgFmt_Arg array by
 *      converting them to offsets relative to the start of the args array.
 *      This should only be invoked if the MsgFmt_Arg array was allocated 
 *      from a caller-supplied buffer from MsgFmt_GetArgsWithBuf.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      For all i such that args[i] is a string parameter,
 *      args[i].v.offset is set to the offset from args to the start
 *      of the string, or to 0 if the string was NULL.
 *
 *-----------------------------------------------------------------------------
 */

void
MsgFmt_SwizzleArgs(MsgFmt_Arg *args,
                   int numArgs)
{
   int i;
   int8* bufStart = (int8*)args;

   for (i = 0; i < numArgs; i++) {
      
      switch (args[i].type) {
         case MSGFMT_ARG_STRING8:
         case MSGFMT_ARG_STRING16:
         case MSGFMT_ARG_STRING32:
	    if (args[i].v.ptr == NULL) {
	       // offset is never 0 otherwise
	       args[i].v.offset = 0;
	    } else {
	       args[i].v.offset = (int8*)args[i].v.ptr - bufStart;
	    }
            break;
         default:
            break;
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmt_GetSwizzledString --
 *
 *      Helper for pointer un-swizzling.  Obtains the pointer encoded
 *      by a swizzled argument, if it is a string and the pointer is
 *      within the proper bounds.
 *
 * Results:
 *      NULL and a non-zero return value if the given argument is not
 *      a string, or the pointer is out of bounds (below the end of
 *      the args array or above the end of the buffer), or the string
 *      is not null-terminated within the buffer.
 *
 *      Exception to the above: an offset of 0 is used to encode the
 *      NULL pointer.  In this case, yields NULL and returns zero.
 *
 *      Otherwise, yields a pointer to the string and returns zero.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
MsgFmt_GetSwizzledString(const MsgFmt_Arg *args,    // IN: argument array
                         int               numArgs, // IN: size of the array
                         int               i,       // IN: index into the array
                         const void       *bufEnd,  // IN: string space bound
                         const int8      **str)     // OUT: the string
{
   const int8 *bufStart = (const int8*)args;
   const int8 *strStart = (const int8*)(args + numArgs);
   const int8 *strEnd = bufEnd;
   
   switch(args[i].type) {
      case MSGFMT_ARG_STRING8:
      case MSGFMT_ARG_STRING16:
      case MSGFMT_ARG_STRING32:
         if (args[i].v.offset == 0) {
            // offset is never 0 otherwise
            *str = NULL;
            return 0;
         } else {
            const int8 *ptr = args[i].v.offset + bufStart;
            
            if (ptr < strStart || ptr >= strEnd
                || memchr(ptr, '\0', strEnd - ptr) == NULL) {
               *str = NULL;
               return -1;
            } else {
               *str = ptr;
               return 0;
            }
         }
         break;
      default:
         *str = NULL;
         return -1;
   }
   NOT_REACHED();
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmt_UnswizzleArgs --
 *
 *      Pointer un-swizzling. Re-instates the pointers in the arg array.
 *      This should only be invoked if the MsgFmt_Arg array was previously
 *      swizzled using MsgFmt_SwizzleArgs.
 *
 *      If a reconstituted pointer would be out of range -- i.e.,
 *      before the end of the args array or after the provided
 *      end-of-buffer pointer -- it is replaced with NULL and an error
 *      is returned.  This is also done if the resulting string is not
 *      null-terminated within the provided bound.
 *
 * Results:
 *      0 on success; -1 in case of bad pointer.
 *
 * Side effects:
 *      For all i such that args[i] is a string parameter, sets
 *      args[i].v.ptr to the string previously encoded as an offset,
 *      or to NULL if the offset was 0, or to NULL in case of error.
 *
 *-----------------------------------------------------------------------------
 */

int
MsgFmt_UnswizzleArgs(MsgFmt_Arg *args,    // IN/OUT: the arguments (+ strings)
                     int         numArgs, // IN: number of arguments
                     void       *bufEnd)  // IN: string space bound
{
   int i;
   int failures = 0;

   for (i = 0; i < numArgs; i++) {
      switch (args[i].type) {
         case MSGFMT_ARG_STRING8:
         case MSGFMT_ARG_STRING16:
         case MSGFMT_ARG_STRING32:
            if (MsgFmt_GetSwizzledString(args, numArgs, i, bufEnd,
                                         (const int8**)&args[i].v.ptr) != 0) {
               ++failures;
            }
            break;
         default:
            break;
      }
   }
   return failures > 0 ? -1 : 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmt_CopyArgs --
 *      
 *      Copy all args from the given 'copyArgs' array. 
 *
 * Results:
 *      Pointer to copied args array.
 *
 * Side effects:
 *      Allocates memory for new args array.
 *
 *-----------------------------------------------------------------------------
 */

MsgFmt_Arg*
MsgFmt_CopyArgs(MsgFmt_Arg* copyArgs,      // IN: Args to be copied
                int numArgs)               // IN: number of args
{
   MsgFmt_Arg *args;
   int i;

   args = malloc(numArgs * sizeof(MsgFmt_Arg));
   if (args == NULL) {
      return NULL;
   }

   memcpy(args, copyArgs, numArgs * sizeof(MsgFmt_Arg));
   
   for (i = 0; i < numArgs; i++) {
      switch (args[i].type) {
         case MSGFMT_ARG_STRING8:
         case MSGFMT_ARG_ERRNO:
	    if (args[i].v.string8 != NULL) {
	       args[i].v.string8char = strdup(copyArgs[i].v.string8char);
	       if (args[i].v.string8 == NULL) {
		  MsgFmt_FreeArgs(args, i);
		  return NULL;
	       }
	    }
            break;
         case MSGFMT_ARG_STRING16:
         case MSGFMT_ARG_STRING32:
            /*
             * We don't care about these types.
             */
            NOT_IMPLEMENTED();
            break;
         default:
            break;
      }
   }

   return args;
}


#ifdef HAS_BSD_PRINTF // {

/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmt_Snprintf --
 *
 *      MsgFmt_Arg version of Str_Vsnprintf().
 *
 * Results:
 *      Number of character written, not including null termination,
 *	or number of characters would have been written on overflow.
 *	(This is exactly the same as vsnprintf(), but different
 *	from Str_Vsnprintf().)
 *	String is always null terminated, even on overflow.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
MsgFmt_Snprintf(char *buf,		 // OUT: formatted string
                size_t size,		 // IN: size of buffer
                const char *format,	 // IN: format
                const MsgFmt_Arg *args,	 // IN: message arguments
                int numArgs)		 // IN: number of arguments
{
   return MsgFmtSnprintfWork(&buf, size, format, args, numArgs);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MsgFmt_Asprintf --
 *
 *      MsgFmt_Arg version of Str_Vasprintf().
 *
 * Results:
 *	Allocated string on success.
 *	NULL on failure.
 *	Length of returned string (not including null termination)
 *	in *length (if length != NULL).
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
MsgFmt_Asprintf(size_t *length,		 // OUT: length of returned string
                const char *format,	 // IN: format
                const MsgFmt_Arg *args,	 // IN: message arguments
                int numArgs)		 // IN: number of arguments
{
   char *p = NULL;
   int n = MsgFmtSnprintfWork(&p, 0, format, args, numArgs);

   if (n < 0) {
      return NULL;
   }
   if (length != NULL) {
      *length = n;
   }
   return p;
}

static int
MsgFmtSnprintfWork(char **outbuf, size_t bufSize, const char *fmt0,
                   const MsgFmt_Arg *args, int numArgs)
{
   char *fmt;      /* format string */
   int ch;         /* character from fmt */
   int n;          /* handy integer (short term usage) */
   char *cp;      /* handy char pointer (short term usage) */
   BSDFmt_IOV *iovp;   /* for PRINT macro */
   int flags;      /* flags as above */
   int ret;      /* return value accumulator */
   int width;      /* width from format (%8d), or 0 */
   int prec;      /* precision from format; <0 for N/A */
   char sign;      /* sign prefix (' ', '+', '-', or \0) */
   char thousands_sep;   /* locale specific thousands separator */
   const char *grouping;   /* locale specific numeric grouping rules */

#ifndef NO_FLOATING_POINT
   /*
    * We can decompose the printed representation of floating
    * point numbers into several parts, some of which may be empty:
    *
    * [+|-| ] [0x|0X] MMM . NNN [e|E|p|P] [+|-] ZZ
    *    A       B     ---C---      D       E   F
    *
    * A:   'sign' holds this value if present; '\0' otherwise
    * B:   ox[1] holds the 'x' or 'X'; '\0' if not hexadecimal
    * C:   cp points to the string MMMNNN.  Leading and trailing
    *   zeros are not in the string and must be added.
    * D:   expchar holds this character; '\0' if no exponent, e.g. %f
    * F:   at least two digits for decimal, at least one digit for hex
    */
   char *decimal_point;   /* locale specific decimal point */
#if defined __ANDROID__
   static char dp = '.';
#endif
   int signflag;      /* true if float is negative */
   union {         /* floating point arguments %[aAeEfFgG] */
      double dbl;
      long double ldbl;
   } fparg;
   int expt;      /* integer value of exponent */
   char expchar;      /* exponent character: [eEpP\0] */
   char *dtoaend;      /* pointer to end of converted digits */
   int expsize;      /* character count for expstr */
   int lead;      /* sig figs before decimal or group sep */
   int ndig;      /* actual number of digits returned by dtoa */
   char expstr[MAXEXPDIG+2];   /* buffer for exponent string: e+ZZZ */
   char *dtoaresult;   /* buffer allocated by dtoa */
   int nseps;      /* number of group separators with ' */
   int nrepeats;      /* number of repeats of the last group */
#endif
   uintmax_t ujval;   /* %j, %ll, %q, %t, %z integers */
   int base;      /* base for [diouxX] conversion */
   int dprec;      /* a copy of prec if [diouxX], 0 otherwise */
   int realsz;      /* field size expanded by dprec, sign, etc */
   int size;      /* size of converted field or string */
   int prsize;             /* max size of printed field */
   const char *xdigs;        /* digits for %[xX] conversion */
   BSDFmt_UIO uio;   /* output information: summary */
   BSDFmt_IOV iov[BSDFMT_NIOV]; /* ... and individual io vectors */
   char buf[INT_CONV_BUF];/* buffer with space for digits of uintmax_t */
   char ox[2];      /* space for 0x; ox[1] is either x, X, or \0 */
   int nextarg;            /* 1-based argument index */
   const MsgFmt_Arg *a;
   char *convbuf;      /* wide to multibyte conversion result */
   BSDFmt_StrBuf sbuf;

   /*
    * BEWARE, these `goto error' on error, and PAD uses `n'.
    */
#define   PRINT(ptr, len) {                             \
      iovp->iov_base = (ptr);                           \
      iovp->iov_len = (len);                            \
      uio.uio_resid += (len);                           \
      iovp++;                                           \
      if (++uio.uio_iovcnt >= BSDFMT_NIOV) {            \
         if (BSDFmt_SPrint(&sbuf, &uio))                \
            goto error;                                 \
         iovp = iov;                                    \
      }                                                 \
   }
#define   PAD(howmany, with) {                  \
      if ((n = (howmany)) > 0) {                \
         while (n > PADSIZE) {                  \
            PRINT(with, PADSIZE);               \
            n -= PADSIZE;                       \
         }                                      \
         PRINT(with, n);                        \
      }                                         \
   }
#define   PRINTANDPAD(p, ep, len, with) do {    \
      int n2 = (ep) - (p);                      \
      if (n2 > (len))                           \
         n2 = (len);                            \
      if (n2 > 0)                               \
         PRINT((p), n2);                        \
      PAD((len) - (n2 > 0 ? n2 : 0), (with));   \
   } while(0)
#define   FLUSH() {                                                     \
      if (uio.uio_resid && BSDFmt_SPrint(&sbuf, &uio))                  \
         goto error;                                                    \
      uio.uio_iovcnt = 0;                                               \
      iovp = iov;                                                       \
   }

#define FETCHARG(a, i) do { \
   int ii = (i) - 1; \
   if (ii >= numArgs) { \
      sbuf.error = TRUE; \
      goto error; \
   } \
   (a) = args + ii; \
} while (FALSE)

   /*
    * Get * arguments, including the form *nn$.
    */
#define GETASTER(val) do { \
   int n2 = 0; \
   char *cp = fmt; \
   const MsgFmt_Arg *a; \
   while (is_digit(*cp)) { \
      n2 = 10 * n2 + to_digit(*cp); \
      cp++; \
   } \
   if (*cp == '$') { \
      FETCHARG(a, n2); \
      fmt = cp + 1; \
   } else { \
      FETCHARG(a, nextarg++); \
   } \
   if (a->type != MSGFMT_ARG_INT32) { \
      sbuf.error = TRUE; \
      goto error; \
   } \
   val = a->v.signed32; \
} while (FALSE)

   xdigs = xdigs_lower;
   thousands_sep = '\0';
   grouping = NULL;
   convbuf = NULL;
#ifndef NO_FLOATING_POINT
   dtoaresult = NULL;
#if defined __ANDROID__
   /*
    * Struct lconv is not working! For decimal_point,
    * using '.' instead is a workaround.
    */
   NOT_TESTED();
   decimal_point = &dp;
#else
   decimal_point = localeconv()->decimal_point;
#endif
#endif

   fmt = (char *)fmt0;
   nextarg = 1;
   uio.uio_iov = iovp = iov;
   uio.uio_resid = 0;
   uio.uio_iovcnt = 0;
   ret = 0;

   /*
    * Set up output string buffer structure.
    */

   sbuf.alloc = *outbuf == NULL;
   sbuf.error = FALSE;
   sbuf.buf = *outbuf;
   sbuf.size = bufSize;
   sbuf.index = 0;

   /*
    * If asprintf(), allocate initial buffer based on format length.
    * Empty format only needs one byte.
    * Otherwise, round up to multiple of 64.
    */

   if (sbuf.alloc) {
      size_t n = strlen(fmt0) + 1;	// +1 for \0
      if (n > 1) {
	 n = ROUNDUP(n, 64);
      }
      if ((sbuf.buf = malloc(n * sizeof (char))) == NULL) {
	 sbuf.error = TRUE;
	 goto error;
      }
      sbuf.size = n;
   }

   // shut compile up
#ifndef NO_FLOATING_POINT
   expt = 0;
   expchar = 0;
   dtoaend = NULL;
   expsize = 0;
   lead = 0;
   ndig = 0;
   nseps = 0;
   nrepeats = 0;
#endif
   ujval = 0;

   /*
    * Scan the format for conversions (`%' character).
    */
   for (;;) {
      for (cp = fmt; (ch = *fmt) != '\0' && ch != '%'; fmt++)
         /* void */;
      if ((n = fmt - cp) != 0) {
         if ((unsigned)ret + n > INT_MAX) {
            ret = EOF;
            goto error;
         }
         PRINT(cp, n);
         ret += n;
      }
      if (ch == '\0')
         goto done;
      fmt++;      /* skip over '%' */

      flags = 0;
      dprec = 0;
      width = 0;
      prec = -1;
      sign = '\0';
      ox[1] = '\0';

     rflag:      ch = *fmt++;
     reswitch:   switch (ch) {
      case ' ':
         /*-
          * ``If the space and + flags both appear, the space
          * flag will be ignored.''
          *   -- ANSI X3J11
          */
         if (!sign)
            sign = ' ';
         goto rflag;
      case '#':
         flags |= ALT;
         goto rflag;
      case '*':
         /*-
          * ``A negative field width argument is taken as a
          * - flag followed by a positive field width.''
          *   -- ANSI X3J11
          * They don't exclude field widths read from args.
          */
         GETASTER (width);
         if (width >= 0)
            goto rflag;
         width = -width;
         /* FALLTHROUGH */
      case '-':
         flags |= LADJUST;
         goto rflag;
      case '+':
         sign = '+';
         goto rflag;
      case '\'':
         flags |= GROUPING;
#if defined __ANDROID__
         /*
          * Struct lconv is not working! The code below is a workaround.
          */
         NOT_TESTED();
         thousands_sep = ',';
#else
         thousands_sep = *(localeconv()->thousands_sep);
         grouping = localeconv()->grouping;
#endif
         goto rflag;
      case '.':
         if ((ch = *fmt++) == '*') {
            GETASTER (prec);
            goto rflag;
         }
         prec = 0;
         while (is_digit(ch)) {
            prec = 10 * prec + to_digit(ch);
            ch = *fmt++;
         }
         goto reswitch;
      case '0':
         /*-
          * ``Note that 0 is taken as a flag, not as the
          * beginning of a field width.''
          *   -- ANSI X3J11
          */
         flags |= ZEROPAD;
         goto rflag;
      case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
         n = 0;
         do {
            n = 10 * n + to_digit(ch);
            ch = *fmt++;
         } while (is_digit(ch));
         if (ch == '$') {
            nextarg = n;
            goto rflag;
         }
         width = n;
         goto reswitch;
      case 'h':
         if (flags & SHORTINT) {
            flags &= ~SHORTINT;
            flags |= CHARINT;
         } else
            flags |= SHORTINT;
         goto rflag;
      case 'j':
         flags |= INTMAXT;
         goto rflag;
      case 'I':
         /* could be I64 - long long int is 64bit */
         if (fmt[0] == '6' && fmt[1] == '4') {
            fmt += 2;
            flags |= LLONGINT;
            goto rflag;
         }
         /* could be I32 - normal int is 32bit */
         if (fmt[0] == '3' && fmt[1] == '2') {
            fmt += 2;
            /* flags |= normal integer - it is 32bit for all our targets */
            goto rflag;
         }
         /*
          * I alone - use Microsoft's semantic as size_t modifier.  We do
          * not support glibc's semantic to use alternative digits.
          */
         flags |= SIZET;
         goto rflag;
      case 'l':
         if (flags & LONGINT) {
            flags &= ~LONGINT;
            flags |= LLONGINT;
         } else
            flags |= LONGINT;
         goto rflag;
      case 'L':
      case 'q':
         flags |= LLONGINT;   /* not necessarily */
         goto rflag;
      case 't':
         flags |= PTRDIFFT;
         goto rflag;
      case 'Z':
      case 'z':
         flags |= SIZET;
         goto rflag;
      case 'C':
         flags |= LONGINT;
         /*FALLTHROUGH*/
      case 'c':
	 FETCHARG(a, nextarg++);
	 if (a->type != MSGFMT_ARG_INT32) {
	    sbuf.error = TRUE;
	    goto error;
	 }
         if (flags & LONGINT) {
            static const mbstate_t initial;
            mbstate_t mbs;
            size_t mbseqlen;

	    mbs = initial;
	    // XXX must deal with mismatch between wchar_t size
            mbseqlen = wcrtomb(cp = buf, (wchar_t)a->v.signed32, &mbs);
            if (mbseqlen == (size_t)-1) {
               sbuf.error = TRUE;
               goto error;
            }
            size = (int)mbseqlen;
         } else {
            *(cp = buf) = a->v.signed32;
            size = 1;
         }
         sign = '\0';
         break;
      case 'D':
         flags |= LONGINT;
         /*FALLTHROUGH*/
      case 'd':
      case 'i':
	 FETCHARG(a, nextarg++);
	 if ((flags & (INTMAXT|LLONGINT)) != 0) {
	    if (a->type == MSGFMT_ARG_INT64) {
	       ujval = a->v.signed64;
	    } else {
	       sbuf.error = TRUE;
	       goto error;
	    }
	 } else if ((flags & (SIZET|PTRDIFFT|LONGINT)) != 0) {
	    if (a->type == MSGFMT_ARG_INT64) {
	       ujval = a->v.signed64;
	    } else if (a->type == MSGFMT_ARG_INT32) {
	       ujval = (intmax_t) a->v.signed32;
	    } else {
	       sbuf.error = TRUE;
	       goto error;
	    }
	 } else if ((flags & SHORTINT) != 0) {
	    if (a->type == MSGFMT_ARG_INT32) {
	       ujval = (intmax_t) (short) a->v.signed32;
	    } else {
	       sbuf.error = TRUE;
	       goto error;
	    }
	 } else if ((flags & CHARINT) != 0) {
	    if (a->type == MSGFMT_ARG_INT32) {
	       ujval = (intmax_t) (signed char) a->v.signed32;
	    } else {
	       sbuf.error = TRUE;
	       goto error;
	    }
	 } else {
	    if (a->type == MSGFMT_ARG_INT32) {
	       ujval = (intmax_t) a->v.signed32;
	    } else {
	       sbuf.error = TRUE;
	       goto error;
	    }
	 }
	 if ((intmax_t)ujval < 0) {
	    ujval = -ujval;
	    sign = '-';
	 }
         base = 10;
         goto number;
#ifndef NO_FLOATING_POINT
      case 'e':
      case 'E':
         expchar = ch;
         if (prec < 0)   /* account for digit before decpt */
            prec = DEFPREC + 1;
         else
            prec++;
         goto fp_begin;
      case 'f':
      case 'F':
         expchar = '\0';
         goto fp_begin;
      case 'g':
      case 'G':
         expchar = ch - ('g' - 'e');
         if (prec == 0)
            prec = 1;
      fp_begin:
         if (flags & LLONGINT) {
	    sbuf.error = TRUE;
	    goto error;
	 }
         if (prec < 0)
            prec = DEFPREC;
         if (dtoaresult != NULL)
            freedtoa(dtoaresult);
	 FETCHARG(a, nextarg++);
	 if (a->type != MSGFMT_ARG_FLOAT64) {
	    sbuf.error = TRUE;
	    goto error;
	 }
	 fparg.dbl = a->v.float64;
#if defined NO_DTOA
         NOT_TESTED();
         dtoaresult = NULL;
         sbuf.error = TRUE;

         goto error;
#else
	 dtoaresult = cp =
	    dtoa(fparg.dbl, expchar ? 2 : 3, prec,
		 &expt, &signflag, &dtoaend);
#endif
	 if (expt == 9999)
	    expt = INT_MAX;
         if (signflag)
            sign = '-';
         if (expt == INT_MAX) {   /* inf or nan */
            if (*cp == 'N') {
               cp = (ch >= 'a') ? "nan" : "NAN";
               sign = '\0';
            } else
               cp = (ch >= 'a') ? "inf" : "INF";
            size = 3;
            break;
         }
         flags |= FPT;
         ndig = dtoaend - cp;
         if (ch == 'g' || ch == 'G') {
            if (expt > -4 && expt <= prec) {
               /* Make %[gG] smell like %[fF] */
               expchar = '\0';
               if (flags & ALT)
                  prec -= expt;
               else
                  prec = ndig - expt;
               if (prec < 0)
                  prec = 0;
            } else {
               /*
                * Make %[gG] smell like %[eE], but
                * trim trailing zeroes if no # flag.
                */
               if (!(flags & ALT))
                  prec = ndig;
            }
         }
         if (expchar) {
            expsize = BSDFmt_Exponent(expstr, expt - 1, expchar);
            size = expsize + prec;
            if (prec > 1 || flags & ALT)
               ++size;
         } else {
            /* space for digits before decimal point */
            if (expt > 0)
               size = expt;
            else   /* "0" */
               size = 1;
            /* space for decimal pt and following digits */
            if (prec || flags & ALT)
               size += prec + 1;
            if (grouping && expt > 0) {
               /* space for thousands' grouping */
               nseps = nrepeats = 0;
               lead = expt;
               while (*grouping != CHAR_MAX) {
                  if (lead <= *grouping)
                     break;
                  lead -= *grouping;
                  if (*(grouping+1)) {
                     nseps++;
                     grouping++;
                  } else
                     nrepeats++;
               }
               size += nseps + nrepeats;
            } else
               lead = expt;
         }
         break;
#endif /* !NO_FLOATING_POINT */
      case 'n':
	 sbuf.error = TRUE;
	 goto error;
      case 'O':
         flags |= LONGINT;
         /*FALLTHROUGH*/
      case 'o':
         base = 8;
         goto get_unsigned;
      case 'p':
         /*-
          * ``The argument shall be a pointer to void.  The
          * value of the pointer is converted to a sequence
          * of printable characters, in an implementation-
          * defined manner.''
          *   -- ANSI X3J11
          */
	 FETCHARG(a, nextarg++);
	 if (a->type == MSGFMT_ARG_PTR32) {
	    ujval = a->v.unsigned32;
	 } else if (a->type == MSGFMT_ARG_PTR64) {
	    ujval = a->v.unsigned64;
	 } else {
	    sbuf.error = TRUE;
	    goto error;
	 }
         base = 16;
         xdigs = xdigs_upper;
         flags = flags | INTMAXT;
         /*
          * PR 103201
          * VisualC sscanf doesn't grok '0x', so prefix zeroes.
          */
//         ox[1] = 'x';
         goto nosign;
      case 'S':
         flags |= LONGINT;
         /*FALLTHROUGH*/
      case 's':
	 FETCHARG(a, nextarg++);
	 if (flags & LONGINT) {
            wchar_t *wcp;
#if defined __ANDROID__
            ASSERT_ON_COMPILE(sizeof (wchar_t) == sizeof (int16) ||
                              sizeof (wchar_t) == sizeof (int32) ||
                              sizeof (wchar_t) == sizeof (int8));
            if ((sizeof (wchar_t) == sizeof (int16) &&
                 a->type != MSGFMT_ARG_STRING16) ||
                (sizeof (wchar_t) == sizeof (int32) &&
                 a->type != MSGFMT_ARG_STRING32) ||
                (sizeof (wchar_t) == sizeof (int8) &&
                 a->type != MSGFMT_ARG_STRING8)) {
#else
            ASSERT_ON_COMPILE(sizeof (wchar_t) == 2 || sizeof (wchar_t) == 4);
	    if (sizeof (wchar_t) == 2 ?
		a->type != MSGFMT_ARG_STRING16 :
		a->type != MSGFMT_ARG_STRING32) {
#endif
	       sbuf.error = TRUE;
	       goto error;
	    }
            if ((wcp = (wchar_t *) a->v.ptr) == NULL)
               cp = "(null)";
            else {
	       if (convbuf != NULL)
		  free(convbuf);
               convbuf = BSDFmt_WCharToUTF8(wcp, prec);
               if (convbuf == NULL) {
                  sbuf.error = TRUE;
                  goto error;
               }
               cp = convbuf;
            }
	 } else {
	    if (a->type != MSGFMT_ARG_STRING8 &&
		a->type != MSGFMT_ARG_ERRNO) {
	       sbuf.error = TRUE;
	       goto error;
	    }

	    /*
	     * Use localized string (in localString) if available.
	     * Strip off Msg ID if unlocalized string has one.
	     * Use (null) for null pointer.
	     */

	    if (a->p.localString != NULL) {
	       cp = a->p.localString;
	    } else if (a->v.string8 != NULL) {
	       cp = (char *) Msg_StripMSGID(a->v.string8char);
	    } else {
	       cp = "(null)";
	    }
	 }
         if (prec >= 0) {
            /*
             * We can use strlen here because the string is always
	     * terminated, unlike the string passed to MsgFmt_GetArgs.
	     * However, it's somewhat faster to use memchr.
	     */
            char *p = memchr(cp, 0, prec);

            if (p != NULL) {
               size = p - cp;
               if (size > prec)
                  size = prec;
            } else
               size = prec;
         } else
            size = strlen(cp);
         sign = '\0';
         break;
      case 'U':
         flags |= LONGINT;
         /*FALLTHROUGH*/
      case 'u':
         base = 10;
         goto get_unsigned;
      case 'X':
         xdigs = xdigs_upper;
         goto hex;
      case 'x':
         xdigs = xdigs_lower;
      hex:
         base = 16;
         if (flags & ALT)
            ox[1] = ch;
         flags &= ~GROUPING;

      get_unsigned:
	 FETCHARG(a, nextarg++);
	 if ((flags & (INTMAXT|LLONGINT)) != 0) {
	    if (a->type == MSGFMT_ARG_INT64) {
	       ujval = a->v.unsigned64;
	    } else {
	       sbuf.error = TRUE;
	       goto error;
	    }
	 } else if ((flags & (SIZET|PTRDIFFT|LONGINT)) != 0) {
	    if (a->type == MSGFMT_ARG_INT64) {
	       ujval = a->v.unsigned64;
	    } else if (a->type == MSGFMT_ARG_INT32) {
	       ujval = (uintmax_t) a->v.unsigned32;
	    } else {
	       sbuf.error = TRUE;
	       goto error;
	    }
	 } else if ((flags & SHORTINT) != 0) {
	    if (a->type == MSGFMT_ARG_INT32) {
	       ujval = (intmax_t) (unsigned short) a->v.unsigned32;
	    } else {
	       sbuf.error = TRUE;
	       goto error;
	    }
	 } else if ((flags & CHARINT) != 0) {
	    if (a->type == MSGFMT_ARG_INT32) {
	       ujval = (intmax_t) (unsigned char) a->v.unsigned32;
	    } else {
	       sbuf.error = TRUE;
	       goto error;
	    }
	 } else {
	    if (a->type == MSGFMT_ARG_INT32) {
	       ujval = (intmax_t) a->v.unsigned32;
	    } else {
	       sbuf.error = TRUE;
	       goto error;
	    }
	 }
         if (ujval == 0) /* squash 0x/X if zero */
            ox[1] = '\0';

         /* unsigned conversions */
      nosign:
	 sign = '\0';
         /*-
          * ``... diouXx conversions ... if a precision is
          * specified, the 0 flag will be ignored.''
          *   -- ANSI X3J11
          */
      number:
	 if ((dprec = prec) >= 0)
            flags &= ~ZEROPAD;

         /*-
          * ``The result of converting a zero value with an
          * explicit precision of zero is no characters.''
          *   -- ANSI X3J11
          *
          * ``The C Standard is clear enough as is.  The call
          * printf("%#.0o", 0) should print 0.''
          *   -- Defect Report #151
          */
         cp = buf + INT_CONV_BUF;
	 if (ujval != 0 || prec != 0 ||
	     (flags & ALT && base == 8))
	    cp = BSDFmt_UJToA(ujval, cp, base,
			      flags & ALT, xdigs,
			      flags & GROUPING, thousands_sep,
			      grouping);
         size = buf + INT_CONV_BUF - cp;
         if (size > INT_CONV_BUF)   /* should never happen */
            abort();
         break;
      default:   /* "%?" prints ?, unless ? is NUL */
         if (ch == '\0')
            goto done;
         /* pretend it was %c with argument ch */
         cp = buf;
         *cp = ch;
         size = 1;
         sign = '\0';
         break;
      }

      /*
       * All reasonable formats wind up here.  At this point, `cp'
       * points to a string which (if not flags&LADJUST) should be
       * padded out to `width' places.  If flags&ZEROPAD, it should
       * first be prefixed by any sign or other prefix; otherwise,
       * it should be blank padded before the prefix is emitted.
       * After any left-hand padding and prefixing, emit zeroes
       * required by a decimal [diouxX] precision, then print the
       * string proper, then emit zeroes required by any leftover
       * floating precision; finally, if LADJUST, pad with blanks.
       *
       * Compute actual size, so we know how much to pad.
       * size excludes decimal prec; realsz includes it.
       */
      realsz = dprec > size ? dprec : size;
      if (sign)
         realsz++;
      if (ox[1])
         realsz += 2;

      prsize = width > realsz ? width : realsz;
      if ((unsigned)ret + prsize > INT_MAX) {
         ret = EOF;
         goto error;
      }

      /* right-adjusting blank padding */
      if ((flags & (LADJUST|ZEROPAD)) == 0)
         PAD(width - realsz, blanks);

      /* prefix */
      if (sign)
         PRINT(&sign, 1);

      if (ox[1]) {   /* ox[1] is either x, X, or \0 */
         ox[0] = '0';
         PRINT(ox, 2);
      }

      /* right-adjusting zero padding */
      if ((flags & (LADJUST|ZEROPAD)) == ZEROPAD)
         PAD(width - realsz, zeroes);

      /* leading zeroes from decimal precision */
      PAD(dprec - size, zeroes);

      /* the string or number proper */
#ifndef NO_FLOATING_POINT
      if ((flags & FPT) == 0) {
         PRINT(cp, size);
      } else {   /* glue together f_p fragments */
         if (!expchar) {   /* %[fF] or sufficiently short %[gG] */
            if (expt <= 0) {
               PRINT(zeroes, 1);
               if (prec || flags & ALT)
                  PRINT(decimal_point, 1);
               PAD(-expt, zeroes);
               /* already handled initial 0's */
               prec += expt;
            } else {
               PRINTANDPAD(cp, dtoaend, lead, zeroes);
               cp += lead;
               if (grouping) {
                  while (nseps>0 || nrepeats>0) {
                     if (nrepeats > 0)
                        nrepeats--;
                     else {
                        grouping--;
                        nseps--;
                     }
                     PRINT(&thousands_sep,
                           1);
                     PRINTANDPAD(cp,dtoaend,
                                 *grouping, zeroes);
                     cp += *grouping;
                  }
                  if (cp > dtoaend)
                     cp = dtoaend;
               }
               if (prec || flags & ALT)
                  PRINT(decimal_point,1);
            }
            PRINTANDPAD(cp, dtoaend, prec, zeroes);
         } else {   /* %[eE] or sufficiently long %[gG] */
            if (prec > 1 || flags & ALT) {
               buf[0] = *cp++;
               buf[1] = *decimal_point;
               PRINT(buf, 2);
               PRINT(cp, ndig-1);
               PAD(prec - ndig, zeroes);
            } else   /* XeYYY */
               PRINT(cp, 1);
            PRINT(expstr, expsize);
         }
      }
#else
      PRINT(cp, size);
#endif
      /* left-adjusting padding (always blank) */
      if (flags & LADJUST)
         PAD(width - realsz, blanks);

      /* finally, adjust ret */
      ret += prsize;

      FLUSH();   /* copy out the I/O vectors */
   }
done:
   FLUSH();

   /*
    * Always null terminate, unless buffer is size 0.
    */

   ASSERT(!sbuf.error && ret >= 0);
   if (sbuf.size <= 0) {
      ASSERT(!sbuf.alloc);
   } else {
      ASSERT(sbuf.index < sbuf.size);
      sbuf.buf[sbuf.index] = '\0';
   }

error:
#ifndef NO_FLOATING_POINT
   if (dtoaresult != NULL)
      freedtoa(dtoaresult);
#endif
   if (convbuf != NULL)
      free(convbuf);
   if (sbuf.error) {
      ret = EOF;
   }

   // return allocated buffer on success, free it on failure
   if (sbuf.alloc) {
      if (ret < 0) {
	 free(sbuf.buf);
      } else {
	 *outbuf = sbuf.buf;
      }
   }

   return (ret);
   /* NOTREACHED */

#undef PRINT
#undef PAD
#undef PRINTANDPAD
#undef FLUSH
#undef FETCHARG
#undef GETASTER
}

#endif // }
