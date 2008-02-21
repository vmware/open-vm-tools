/*********************************************************
 * Copyright (C) 2000 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * balloon_def.h -- 
 *
 *	Definitions for server "balloon" mechanism for reclaiming
 *	physical memory from a VM.
 */

#ifndef	_BALLOON_DEF_H
#define	_BALLOON_DEF_H

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#define	INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#include "vm_basic_types.h"

/*
 * constants
 */

/* backdoor port */
#define	BALLOON_BDOOR_PORT		(0x5670)
#define	BALLOON_BDOOR_MAGIC		(0x456c6d6f)

/* backdoor command numbers */
#define	BALLOON_BDOOR_CMD_START		(0)
#define	BALLOON_BDOOR_CMD_TARGET	(1)
#define	BALLOON_BDOOR_CMD_LOCK		(2)
#define	BALLOON_BDOOR_CMD_UNLOCK	(3)
#define	BALLOON_BDOOR_CMD_GUEST_ID	(4)

/* use config value for max balloon size */
#define BALLOON_MAX_SIZE_USE_CONFIG	(0)

/* guest identities */
#define BALLOON_GUEST_UNKNOWN		(0)
#define BALLOON_GUEST_LINUX		(1)
#define BALLOON_GUEST_BSD		(2)
#define BALLOON_GUEST_WINDOWS_NT4	(3)
#define BALLOON_GUEST_WINDOWS_NT5	(4)
#define BALLOON_GUEST_SOLARIS		(5)

/* error codes */
#define	BALLOON_SUCCESS			(0)
#define	BALLOON_FAILURE			(-1)
#define	BALLOON_ERROR_CMD_INVALID	(1)
#define	BALLOON_ERROR_PPN_INVALID	(2)
#define	BALLOON_ERROR_PPN_LOCKED	(3)
#define	BALLOON_ERROR_PPN_UNLOCKED	(4)
#define	BALLOON_ERROR_PPN_PINNED	(5)
#define	BALLOON_ERROR_PPN_TRANSPARENT	(6)
#define	BALLOON_ERROR_RESET		(7)
#define	BALLOON_ERROR_BUSY		(8)

/*
 * types
 */

typedef struct {
   // platform -> VMM
   uint32 target;	// target balloon size (in pages)

   // platform <- VMM
   uint32 size;		// current balloon size (in pages)
   uint32 nOps;		// stats: operation count
   uint32 nReset;	// stats: reset count
   uint32 guestType;	// guest OS identifier
   uint32 maxSize;	// predicted max balloon size (in pages)
} Balloon_BalloonInfo;

#endif	/* _BALLOON_DEF_H */
