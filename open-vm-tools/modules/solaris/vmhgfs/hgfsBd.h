/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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

#ifndef _HGFS_BD_H_
# define _HGFS_BD_H_

/*
 * hgfsBd.h --
 *
 *    Backdoor calls used by hgfs clients.
 */

#include "rpcout.h"

char *HgfsBd_GetBuf(void);

char *HgfsBd_GetLargeBuf(void);

void HgfsBd_PutBuf(char *);

RpcOut *HgfsBd_GetChannel(void);

Bool HgfsBd_CloseChannel(RpcOut *out);

int HgfsBd_Dispatch(RpcOut *out,
                    char *packetIn,
                    size_t *packetSize,
                    char const **packetOut);

Bool HgfsBd_Enabled(RpcOut *out,
                    char *requestPacket);

Bool HgfsBd_OpenBackdoor(RpcOut **out);

Bool HgfsBd_CloseBackdoor(RpcOut **out);

#endif // _HGFS_BD_H_
