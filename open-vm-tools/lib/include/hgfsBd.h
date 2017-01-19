/*********************************************************
 * Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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

/*********************************************************
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
