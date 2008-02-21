/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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

#ifndef _HGFS_BD_H_
# define _HGFS_BD_H_

/*
 * hgfsBd.h --
 *
 *    Backdoor calls used by hgfs clients.
 */

#include "rpcout.h"

char *HgfsBd_GetBuf(void);

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
