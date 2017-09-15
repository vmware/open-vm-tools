/*********************************************************
 * Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
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
 * stub-msgfmt-fbsd.c --
 *
 *    FreeBSD-specific stubs for lib/misc/msgfmt.c. This stubs out two
 *    specific funtions in that file - MsgFmt_Asprintf and MsgFmt_Snprintf,
 *    which don't have FreeBSD implementations in our code base.
 *
 *    Tools don't really use the Msg_* functions for error reporting and etc,
 *    so this is easier than getting all that stuff to compile on FreeBSD.
 *
 *    The stubs won't assert, but they're sort of dumb: they'll just
 *    print the format string with no substitutions to the output.
 */

#include "msgfmt.h"
#include "str.h"

int
MsgFmt_Snprintf(char *buf,                // OUT: formatted string
                size_t size,              // IN: size of buffer
                const char *format,       // IN: format
                const MsgFmt_Arg *args,   // IN: message arguments
                int numArgs)              // IN: number of arguments
{
   return Str_Snprintf(buf, size, "%s", format);
}


char *
MsgFmt_Asprintf(size_t *length,           // OUT: length of returned string
                const char *format,       // IN: format
                const MsgFmt_Arg *args,   // IN: message arguments
                int numArgs)              // IN: number of arguments
{
   return Str_Asprintf(length, "%s", format);
}

