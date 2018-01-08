/*********************************************************
 * Copyright (C) 2007-2017 VMware, Inc. All rights reserved.
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
 * vm_ctype.h -- wrappers to <ctype.h> calls which are vulnerable on Windows implementation
 */

#ifndef _VM_CTYPE_H_
#define _VM_CTYPE_H_

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include <ctype.h>

#if defined __cplusplus
extern "C" {
#endif


/*
 * On Windows platform, ctype.h functions are implemented via table lookup,
 * and a negative index is unsafe.  See bug 83950.
 * Attack the parameter with 0xFF to cast away the signedness.
 */

#ifdef _WIN32

#define CType_IsAlnum(c) isalnum((c) & 0xFF)
#define CType_IsAlpha(c) isalpha((c) & 0xFF)
#define CType_IsAscii(c) isascii((c) & 0xFF)
static __inline int CType_IsBlank(int c) { return c == ' ' || c == '\t'; }
#define CType_IsCntrl(c) iscntrl((c) & 0xFF)
#define CType_IsDigit(c) isdigit((c) & 0xFF)
#define CType_IsGraph(c) isgraph((c) & 0xFF)
#define CType_IsLower(c) islower((c) & 0xFF)
#define CType_IsPrint(c) isprint((c) & 0xFF)
#define CType_IsPunct(c) ispunct((c) & 0xFF)
#define CType_IsSpace(c) isspace((c) & 0xFF)
#define CType_IsUpper(c) isupper((c) & 0xFF)
#define CType_IsXDigit(c) isxdigit((c) & 0xFF)

#else

#define CType_IsAlnum(c) isalnum((c))
#define CType_IsAlpha(c) isalpha((c))
#define CType_IsAscii(c) isascii((c))
#define CType_IsBlank(c) isblank((c))
#define CType_IsCntrl(c) iscntrl((c))
#define CType_IsDigit(c) isdigit((c))
#define CType_IsGraph(c) isgraph((c))
#define CType_IsLower(c) islower((c))
#define CType_IsPrint(c) isprint((c))
#define CType_IsPunct(c) ispunct((c))
#define CType_IsSpace(c) isspace((c))
#define CType_IsUpper(c) isupper((c))
#define CType_IsXDigit(c) isxdigit((c))

#endif /* _WIN32 */


#if defined __cplusplus
}
#endif

#endif // _VM_CTYPE_H_
