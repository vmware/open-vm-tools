/*********************************************************
 * Copyright (C) 2006-2019 VMware, Inc. All rights reserved.
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
 * hgfsclient.c --
 *
 *      Userspace HGFS client. Will one day be as full featured as an HGFS
 *      filesystem driver.
 *
 */

#define G_LOG_DOMAIN "hgfsClient"
#include "vmware.h"
#include "guestApp.h"
#include "vmcheck.h"
#include "escBitvector.h"
#include "hgfsEscape.h"
#include "hgfs.h"
#include "hgfsBd.h"
#include "hgfsProto.h"
#include "conf.h"
#include "str.h"
#include "vmware/tools/log.h"
#include "vmware/tools/utils.h"
#ifdef _WIN32
#include "vmware/tools/win32util.h"
#endif

#include "hgfsclient_version.h"
#include "vm_version.h"
#include "embed_version.h"
VM_EMBED_VERSION(HGFSCLIENT_VERSION_STRING);

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

RpcOut *gChannel = NULL;
char *gPacketBuffer = NULL;

static Bool HgfsClient_Open(HgfsHandle *rootHandle);
static HgfsFileName *HgfsClient_Read(HgfsHandle rootHandle,
                                     int offset);
static Bool HgfsClient_Close(HgfsHandle rootHandle);
static Bool HgfsClient_PrintShares(void);
static Bool HgfsClient_Init(void);
static Bool HgfsClient_Cleanup(void);


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsClient_Open --
 *
 *    Open the root directory on the host.
 *
 * Results:
 *    TRUE on success. FALSE otherwise. When TRUE, the root directory handle
 *    is returned as an argument.
 *
 * Side effects:
 *    The host has cached an open search for us.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsClient_Open(HgfsHandle *rootHandle) // OUT: Handle to root directory
{
   Bool success = FALSE;
   HgfsRequestSearchOpen *searchOpenReq;
   HgfsReplySearchOpen *searchOpenRep = NULL;
   int err;
   char const *replyPacket;
   size_t packetSize;

   /* Create a SearchOpen and send it. */
   searchOpenReq = (HgfsRequestSearchOpen *)gPacketBuffer;
   memset(searchOpenReq, 0, sizeof *searchOpenReq);
   searchOpenReq->header.id = 0;
   searchOpenReq->header.op = HGFS_OP_SEARCH_OPEN;
   searchOpenReq->dirName.length = 0;
   searchOpenReq->dirName.name[0] = 0;
   packetSize = sizeof *searchOpenReq;

   err = HgfsBd_Dispatch(gChannel, (char *)searchOpenReq, 
                         &packetSize, &replyPacket);
   if (err != 0) {
      Warning("Failed to send search open request.\n");
      goto out;
   }
   
   /* replyPacket has our search handle. */
   searchOpenRep = (HgfsReplySearchOpen *)replyPacket;
   if (searchOpenRep->header.status != HGFS_STATUS_SUCCESS) {
      Warning("Error in opening root directory.\n");
      goto out;
   }
   success = TRUE;

  out:
   /* We got the root handle. */
   if (success) {
      *rootHandle = searchOpenRep->search;
   }
   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsClient_Read --
 *
 *    Read a share name from the host.
 *
 * Results:
 *    Pointer into the packet buffer where the caller can find the
 *    HgfsFileName struct. Since this is a pointer into the global
 *    packet buffer, the caller should not free it.
 *
 *    NULL if there was an error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static HgfsFileName *
HgfsClient_Read(HgfsHandle rootHandle, // IN: Handle to root directory
                int offset)            // IN: Offset of dirent to read
{
   HgfsFileName *shareName = NULL;
   HgfsRequestSearchRead *searchReadReq;
   HgfsReplySearchRead *searchReadRep;
   int err;
   char const *replyPacket;
   size_t packetSize;

   /* Create searchRead and send it. */
   searchReadReq = (HgfsRequestSearchRead *)gPacketBuffer;
   memset(searchReadReq, 0, sizeof *searchReadReq);
   searchReadReq->header.id = 0;
   searchReadReq->header.op = HGFS_OP_SEARCH_READ;
   searchReadReq->search = rootHandle;
   searchReadReq->offset = offset;
   packetSize = sizeof *searchReadReq;

   err = HgfsBd_Dispatch(gChannel, (char *)searchReadReq, 
                         &packetSize, &replyPacket);
   if (err != 0) {
      Warning("Failed to send search read request.\n");
      goto out;
   }

   /* replyPacket has our share name. */
   searchReadRep = (HgfsReplySearchRead *)replyPacket;
   if (searchReadRep->header.status != HGFS_STATUS_SUCCESS) {
      Warning("Error in getting share name.\n");
      goto out;
   }

   /* We got the share name. */
   shareName = &searchReadRep->fileName;

  out:
   return shareName;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsClient_Close --
 *
 *    Closes the root directory on the host.
 *
 * Results:
 *    TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *    Host releases state on our opened search.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsClient_Close(HgfsHandle rootHandle) // IN: Handle to root directory
{
   HgfsRequestSearchClose *searchCloseReq;
   HgfsReplySearchClose *searchCloseRep;
   int err;
   char const *replyPacket;
   size_t packetSize;

   /* Create a SearchClose and send it. */
   searchCloseReq = (HgfsRequestSearchClose *)gPacketBuffer;
   memset(searchCloseReq, 0, sizeof *searchCloseReq);
   searchCloseReq->header.id = 0;
   searchCloseReq->header.op = HGFS_OP_SEARCH_CLOSE;
   searchCloseReq->search = rootHandle;
   packetSize = sizeof *searchCloseReq;

   err = HgfsBd_Dispatch(gChannel, (char *)searchCloseReq, 
                         &packetSize, &replyPacket);
   if (err != 0) {
      Warning("Failed to send search close request.\n");
      return FALSE;
   }

   /* replyPacket has success/failure. */
   searchCloseRep = (HgfsReplySearchClose *)replyPacket;
   if (searchCloseRep->header.status != HGFS_STATUS_SUCCESS) {
      Warning("Error closing root directory.\n");
      return FALSE;
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsClient_PrintShares --
 *
 *    List all the shares available on the host.
 *
 * Results:
 *    TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsClient_PrintShares(void)
{
   Bool success = FALSE;
   int offset = 0;
   char escapedName[PATH_MAX + 1];
   HgfsHandle rootHandle;
   HgfsFileName *fileName;

   if (!HgfsClient_Open(&rootHandle)) {
      return success;
   }

   while (TRUE) {
      fileName = HgfsClient_Read(rootHandle, offset++);
      if (fileName == NULL) {
         break;
      }
  
      /* Are we done? */
      if (fileName->length == 0) {
         success = TRUE;
         break;
      }

      /* 
       * Escape this filename. If we get back a negative result, it means that
       * the escaped filename is too big, so skip this share.
       */
      if (HgfsEscape_Do(fileName->name, fileName->length,
                           sizeof escapedName, escapedName) < 0) {
         continue;
      } 

      /* Skip "." and ".." which can be returned. */
      if (strcmp(".", escapedName) == 0 ||
          strcmp("..", escapedName) == 0) {
         continue;
      }
      printf("%s\n", escapedName);

   }
   
   if (!HgfsClient_Close(rootHandle)) {
      success = FALSE;
   }
   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsClient_Init --
 *
 *    Do some initialization "stuff".
 *
 * Results:
 *    TRUE if initialization succeeded, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsClient_Init(void)
{
   Bool success = FALSE;
   GKeyFile *conf = NULL;

   VMTools_LoadConfig(NULL, G_KEY_FILE_NONE, &conf, NULL);
   VMTools_ConfigLogging("hgfsclient", conf, FALSE, FALSE);
   if (conf != NULL) {
      g_key_file_free(conf);
      conf = NULL;
   }

   if (!VmCheck_IsVirtualWorld()) {
      Warning("This application must be run in a Virtual Machine.\n");
      goto out;
   }

   /* Setup an HGFS channel and packet buffer. */   
   gChannel = HgfsBd_GetChannel();
   if (gChannel == NULL) {
      Warning("Failed to create RPC channel\n");
      goto out;
   }
   gPacketBuffer = HgfsBd_GetBuf();
   if (gPacketBuffer == NULL) {
      Warning("Failed to create packet buffer\n");
      goto out;
   }

   /* Find out if HGFS is enabled in the VMX. */
   if (!HgfsBd_Enabled(gChannel, gPacketBuffer)) {
      Warning("HGFS is disabled in the host\n");
      goto out;
   }
   success = TRUE;

  out:
   if (!success) {
      HgfsClient_Cleanup();
   }
   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsClient_Cleanup --
 *
 *    Do some cleanup crap.
 *
 * Results:
 *    TRUE if cleanup succeeded, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsClient_Cleanup(void)
{
   Bool success = TRUE;

   if (gPacketBuffer != NULL) {
      HgfsBd_PutBuf(gPacketBuffer);
   }
   if (gChannel != NULL) {
      if (!HgfsBd_CloseChannel(gChannel)) {
         Warning("Failed to close RPC channel\n");
         success = FALSE;
      }
   }
   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * main --
 *
 *    Main entry point. Calls into the host's HGFS server and prints out
 *    a list of the available shares.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
main(int argc,          // IN
     char *argv[])      // IN
{
#ifdef _WIN32
   WinUtil_EnableSafePathSearching(TRUE);
#endif
   if (argc == 2 &&
       (!strncmp(argv[1], "-h", 2) ||
        !strncmp(argv[1], "--help", 6))) {
      fprintf(stderr, "hgfsclient: lists any shared folders.\n");
      return 0;
   }

   if (!HgfsClient_Init()) {
      return EXIT_FAILURE;
   }
   if (!HgfsClient_PrintShares()) {
      return EXIT_FAILURE;
   }
   if (!HgfsClient_Cleanup()) {
      return EXIT_FAILURE;
   }
   return EXIT_SUCCESS;
}

