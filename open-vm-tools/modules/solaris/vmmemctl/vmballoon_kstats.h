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
 * vmballoon_kstats.h --
 *
 *	External definitions associated with the functions providing
 *	kstats for the vmmemctl driver.
 */

#ifndef VMBALLOON_KSTATS_H
#define VMBALLOON_KSTATS_H

extern kstat_t *BalloonKstatCreate(void);
extern void BalloonKstatDelete(kstat_t *);

#endif /* VMBALLOON_KSTATS_H */
