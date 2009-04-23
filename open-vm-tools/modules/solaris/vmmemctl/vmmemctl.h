/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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
 * vmmemctl.h --
 *
 *	Definitions for the interface between vmmemctl daemon (vmmemctld)
 *	and driver.
 */

#ifndef _VMMEMCTL_H
#define _VMMEMCTL_H

#define	VMMIOC		(0xba << 8)		/* prefix */
#define VMMIOCWORK	(VMMIOC | 0x01)

#endif /* _VMMEMCTL_H */
