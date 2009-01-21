/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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
 * device.h --
 *
 * Definitions and includes for device driver code.
 *
 */


#ifndef __DEVICE_H_
#define __DEVICE_H_

#include <sys/open.h>           /* OTYP_CHR */
#include <sys/stat.h>           /* S_IFCHR */
#include <sys/ddi.h>            /* Device Driver Interface */
#include <sys/sunddi.h>         /* Sun DDI defines (DDI_PSEUDO) */
#include <sys/ksynch.h>         /* mutex and condition variables */

#include "debug.h"

/*
 * Macros
 */

/* Flags for chpoll(): we don't distinguish between these priorities of data */
#define HGFS_POLL_READ          (POLLIN | POLLRDNORM | POLLRDBAND)
#define HGFS_POLL_WRITE         (POLLOUT | POLLWRNORM | POLLWRBAND)


#endif /* __DEVICE_H_ */
