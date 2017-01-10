/*********************************************************
 * Copyright (C) 2009-2016 VMware, Inc. All rights reserved.
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
 * bdhandler.h --
 *
 * Glue for backdoor library.
 */

#ifndef _HGFS_BD_GLUE_H_
#define _HGFS_BD_GLUE_H_

int HgfsBackdoorSendRequest(HgfsReq *req);
void HgfsBackdoorCancelRequest(HgfsReq *req);
Bool HgfsBackdoorInit(void);
void HgfsBackdoorCleanup(void);

#endif // _HGFS_DRIVER_BDHANDLER_H_
