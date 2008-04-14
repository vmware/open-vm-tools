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


#define COPYRIGHT_YEARS    "1998-2008"
#define COPYRIGHT_STRING   "Copyright \251 " COPYRIGHT_YEARS " " COMPANY_NAME
#define UTF8_COPYRIGHT_STRING   "Copyright \302\251 " COPYRIGHT_YEARS " " COMPANY_NAME
#define RIGHT_RESERVED     "All rights reserved."
#define PATENTS_STRING     "Protected by one or more U.S. Patent Nos.\n6,397,242, 6,496,847, 6,704,925, 6,711,672, 6,725,289, 6,735,601,\n6,785,886, 6,789,156, 6,795,966, 6,880,022, 6,944,699, 6,961,806,\n6,961,941, 7,069,413, 7,082,598, 7,089,377, 7,111,086, 7,111,145,\n7,117,481, 7,149,843, 7,155,558, 7,222,221, 7,260,815, 7,260,820,\n7,269,683, 7,275,136, 7,277,998, 7,277,999, 7,278,030, 7,281,102\nand 7,290,253; patents pending."
#define TRADEMARK_STRING   "VMware, the VMware \"boxes\" logo and design, Virtual SMP and VMotion are\nregistered trademarks or trademarks of VMware, Inc. in the United States\nand/or other jurisdictions."
#define GENERIC_TRADEMARK_STRING "All other marks and names mentioned herein may be trademarks of their\nrespective companies."

#endif /* VM_LEGAL_H */
