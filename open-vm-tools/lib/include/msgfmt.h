/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * msgfmg.h --
 *
 *	MsgFmt: format messages for the Msg module
 */

#ifndef _MSGFMT_H_
#define _MSGFMT_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#ifndef VMKERNEL
#include "str.h" // for HAS_BSD_PRINTF
#endif


/*
 * Format parser callback functions
 */

typedef int
MsgFmt_LitFunc(void *clientData, // IN
               char const *buf,  // IN
               int bufSize);     // IN

typedef int
MsgFmt_SpecFunc(void *clientData,       // IN
                char const *pos,        // IN
                unsigned int posSize,   // IN
                char const *type,       // IN
                unsigned int typeSize); // IN


/*
 * Format specifier flags from MsgFmt_ParseSpec()
 */

#define MSGFMT_FLAG_ALT		0x0001
#define MSGFMT_FLAG_ZERO	0x0002
#define MSGFMT_FLAG_MINUS	0x0004
#define MSGFMT_FLAG_SPACE	0x0008
#define MSGFMT_FLAG_PLUS	0x0010
#define MSGFMT_FLAG_QUOTE	0x0020


/*
 * A format argument
 */

typedef enum MsgFmt_ArgType {
   MSGFMT_ARG_INVALID, // must be 0
   MSGFMT_ARG_INT32,
   MSGFMT_ARG_INT64,
   MSGFMT_ARG_PTR32,
   MSGFMT_ARG_PTR64,
   MSGFMT_ARG_FLOAT64,
   MSGFMT_ARG_STRING8,
   MSGFMT_ARG_STRING16,
   MSGFMT_ARG_STRING32,
} MsgFmt_ArgType;

typedef struct MsgFmt_Arg {
   MsgFmt_ArgType type;
   int precision;	// private
   union {
      int32 signed32;
      int64 signed64;
      uint32 unsigned32;
      uint64 unsigned64;
      double float64;
      int8 *string8;
      int16 *string16;
      int32 *string32;
      int32 offset;

      void *ptr;	// private
   } v;
} MsgFmt_Arg;


/*
 * Global functions
 */

typedef int
MsgFmt_ParseFunc(MsgFmt_LitFunc *litFunc,    // IN
                 MsgFmt_SpecFunc *specFunc,  // IN
                 void *clientData,           // IN
                 char const *in);            // IN

MsgFmt_ParseFunc MsgFmt_Parse;
MsgFmt_ParseFunc MsgFmt_ParseWin32;

int
MsgFmt_ParseSpec(char const *pos,       // IN: n$ location
                 unsigned int posSize,  // IN: n$ length
                 char const *type,      // IN: flags, width, etc.
                 unsigned int typeSize, // IN: size of above
		 int *position,         // OUT: argument position
		 int *flags,            // OUT: flags, see MSGFMT_FLAG_*
		 int *width,            // OUT: width
		 int *precision,        // OUT: precision
		 char *lengthMod,       // OUT: length modifier
		 char *conversion);     // OUT: conversion specifier

Bool MsgFmt_GetArgs(const char *fmt, va_list va,
                    MsgFmt_Arg **args, int *numArgs, char **error);
Bool MsgFmt_GetArgsWithBuf(const char *fmt, va_list va,
                           MsgFmt_Arg **args, int *numArgs, char **error,
			   void *buf, size_t bufSize);
void MsgFmt_FreeArgs(MsgFmt_Arg *args, int numArgs);

#ifdef HAS_BSD_PRINTF
int MsgFmt_Snprintf(char *buf, size_t size, const char *format,
                    const MsgFmt_Arg *args, int numArgs);
char *MsgFmt_Asprintf(size_t *length, const char *format,
                      const MsgFmt_Arg *args, int numArgs);
#endif


#endif // ifndef _MSGFMT_H_
