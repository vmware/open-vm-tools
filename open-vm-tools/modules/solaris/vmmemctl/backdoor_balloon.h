/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
 *
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
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
