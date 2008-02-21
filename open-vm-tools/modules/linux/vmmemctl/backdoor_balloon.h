/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * backdoor_balloon.h --
 *
 *    This file provides a wrapper for using the more generic backdoor library
 *    together with the vmballoon-specific backdoor.
 */

#ifndef _BACKDOOR_BALLOON_H_
#define _BACKDOOR_BALLOON_H_

#include "backdoor.h"
#include "balloon_def.h"

static INLINE
void Backdoor_Balloon(Backdoor_proto *myBp) {
   myBp->in.ax.word = BALLOON_BDOOR_MAGIC;
   myBp->in.dx.halfs.low = BALLOON_BDOOR_PORT;
   Backdoor_InOut(myBp);
}

#endif /* _BACKDOOR_BALLOON_H_ */
