/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

#ifndef _HGFS_SERVER_MANAGER_H_
# define _HGFS_SERVER_MANAGER_H_

/*
 * hgfsServerManager.h --
 *
 *    Common routines needed to register an HGFS server.
 */

/* Data structures shared by hgfsServerManagerHost and hgfsServer. */
typedef void (*HgfsServerReplyFunc)(const unsigned char *, unsigned int, void *);

typedef struct ServerRequestRpcContext {
   HgfsServerReplyFunc cb;
   void *cbData;
   char *request;
} ServerRequestRpcContext;

/* 
 * XXX: Gross hack. This variable should not be exposed beyond the HGFS server,
 * but we're under time constraints for bug 143548.
 *
 * In the future, perhaps HgfsServerManager should behave as a device and
 * implement PowerOn/PowerOff/Checkpoint functions, then this can be cleanly
 * abstracted.
 */
extern uint32 hgfsHandleCounter;


Bool HgfsServerManager_Register(void *rpcIn,
                                const char *appName);

void HgfsServerManager_Unregister(void *rpcIn,
                                  const char *appName);

Bool HgfsServerManager_SendRequest(char *request,
                                   uint32 requestSize,
                                   HgfsServerReplyFunc cb,
                                   void *cbData);

Bool HgfsServerManager_CapReg(const char *appName,
                              Bool enable);

#endif // _HGFS_SERVER_MANAGER_H_
