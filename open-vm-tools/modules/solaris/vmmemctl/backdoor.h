/*********************************************************
 * Copyright (C) 1999 VMware, Inc. All rights reserved.
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
 * backdoor.h --
 *
 *    First layer of the internal communication channel between guest
 *    applications and vmware
 */

#ifndef _BACKDOOR_H_
#define _BACKDOOR_H_

#include "vm_basic_types.h"
#include "vm_assert.h"

#include "backdoor_types.h"

void
Backdoor(Backdoor_proto *bp); // IN/OUT

void 
Backdoor_InOut(Backdoor_proto *bp); // IN/OUT

void
Backdoor_HbOut(Backdoor_proto_hb *bp); // IN/OUT

void
Backdoor_HbIn(Backdoor_proto_hb *bp); // IN/OUT

#endif /* _BACKDOOR_H_ */
