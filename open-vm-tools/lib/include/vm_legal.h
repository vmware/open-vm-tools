/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * All the legalese that we display in About boxes and similar places.
 */


#ifndef VM_LEGAL_H
#define VM_LEGAL_H

/*
 * COMPANY_NAME comes from vm_version.h
 */
#include "vm_version.h"


#define COPYRIGHT_YEARS    "1998-2009"
#define COPYRIGHT_STRING   "Copyright \251 " COPYRIGHT_YEARS " " COMPANY_NAME
#define UTF8_COPYRIGHT_STRING   "Copyright \302\251 " COPYRIGHT_YEARS " " COMPANY_NAME
#define GENERIC_COPYRIGHT_STRING   "Copyright (C) " COPYRIGHT_YEARS " " COMPANY_NAME
#define RIGHT_RESERVED     "All rights reserved."

/*
 * Note about the newlines: Keep at most 6 patents per line, but be careful
 * with the last line which will have "; patents pending" appended.
 *
 * Deprecated.  Don't use these anymore.
 */
#define PATENTS_LIST "6,075,938, 6,397,242, 6,496,847, 6,704,925, 6,711,672, 6,725,289,\n6,735,601, 6,785,886, 6,789,156, 6,795,966, 6,880,022, 6,944,699,\n6,961,806, 6,961,941, 7,069,413, 7,082,598, 7,089,377, 7,111,086,\n7,111,145, 7,117,481, 7,149,843, 7,155,558, 7,222,221, 7,260,815,\n7,260,820, 7,269,683, 7,275,136, 7,277,998, 7,277,999, 7,278,030,\n7,281,102, 7,290,253, 7,356,679, 7,409,487, 7,412,492, 7,412,702,\n7,424,710, 7,428,636, 7,433,951, 7,434,002, 7,447,854"
#define PATENTS_STRING_OLD "Protected by one or more U.S. Patent Nos.\n" PATENTS_LIST " and patents pending."

/*
 * Use PATENTS_STRING for showing the patents string in plaintext form.
 * PATENTS_FMT_STRING can be used with PATENTS_URL for creating hyperlinks.
 */
#define PATENTS_STRING_BASE "This product is protected by U.S. and international copyright and\nintellectual property laws. This product is covered by one or more\npatents listed at "
#define PATENTS_STRING PATENTS_STRING_BASE "<" PATENTS_URL ">."
#define PATENTS_FMT_STRING PATENTS_STRING_BASE "%s."
#define PATENTS_URL "http://www.vmware.com/go/patents"

#define TRADEMARK_STRING   "VMware, the VMware \"boxes\" logo and design, Virtual SMP and VMotion are\nregistered trademarks or trademarks of VMware, Inc. in the United States\nand/or other jurisdictions."
#define GENERIC_TRADEMARK_STRING "All other marks and names mentioned herein may be trademarks of their\nrespective companies."

#endif /* VM_LEGAL_H */
