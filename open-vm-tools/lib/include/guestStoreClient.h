/*********************************************************
 * Copyright (C) 2020 VMware, Inc. All rights reserved.
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

/*
 *  guestStoreClient.h  --
 *
 *  Wrapper functions to load the GuestStore libraries.
 */

#ifndef _GUEST_STORE_CLIENT_H_
#define _GUEST_STORE_CLIENT_H_

#include "vmware/tools/guestStoreClientLib.h"

typedef GuestStoreLibError GuestStoreClientError;

/*
 * Caller provided callback to get total content size in bytes and so far
 * received bytes. Return FALSE to cancel content download.
 */
typedef GuestStore_GetContentCallback GuestStoreClient_GetContentCb;

gboolean
GuestStoreClient_Init(void);

gboolean
GuestStoreClient_DeInit(void);

GuestStoreClientError
GuestStoreClient_GetContent(const char *contentPath,
                            const char *outputPath,
                            GuestStoreClient_GetContentCb getContentCb,
                            void *clientCbData);

#endif /* _GUEST_STORE_CLIENT_H_ */