/*********************************************************
 * Copyright (C) 2011-2016, 2019, 2021 VMware, Inc. All rights reserved.
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
 * @file comm.c
 *
 * Client communcation support
 */

#include <stdlib.h>
#include <string.h>

#include "VGAuthInt.h"
#include "VGAuthProto.h"
#include "usercheck.h"


/*
 ******************************************************************************
 * VGAuth_IsConnectedToServiceAsUser --                                  */ /**
 *
 * Checks if the context has a connection to the service.
 *
 * @param[in]  ctx        The VGAuthContext.
 * @param[in]  userName   The user.
 *
 * @return TRUE if the user has a connection to the service.
 *
 ******************************************************************************
 */

gboolean
VGAuth_IsConnectedToServiceAsUser(VGAuthContext *ctx,
                                  const char *userName)
{
   /*
    * If we have a connection, and the user is correct, then we're
    * set.
    */
   return ctx->comm.connected &&
      Usercheck_CompareByName(userName, ctx->comm.userName);
}


/*
 ******************************************************************************
 * VGAuth_IsConnectedToServiceAsAnyUser --                               */ /**
 *
 * Checks if the context has a connection to the service.
 *
 * @param[in]  ctx        The VGAuthContext.
 *
 * @return TRUE if there is a connection to the service.
 *
 ******************************************************************************
 */

gboolean
VGAuth_IsConnectedToServiceAsAnyUser(VGAuthContext *ctx)
{
   return ctx->comm.connected;
}


/*
 ******************************************************************************
 * VGAuth_InitConnection --                                              */ /**
 *
 * Initializes the connection.
 *
 * @param[in]  ctx        The VGAuthContext.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_InitConnection(VGAuthContext *ctx)
{
#ifdef _WIN32
   ctx->comm.hPipe = INVALID_HANDLE_VALUE;
#else
   /*
    * Be sure to init to a bad fd.  Closing stdin is Bad.
    */
   ctx->comm.sock = -1;
#endif

   ctx->comm.connected = FALSE;
   ctx->comm.sequenceNumber = 0;

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * VGAuth_CloseConnection --                                             */ /**
 *
 * Closes the connection.
 *
 * @param[in]  ctx        The VGAuthContext.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_CloseConnection(VGAuthContext *ctx)
{
   VGAuthError err = VGAUTH_E_OK;

   if (NULL == ctx) {
      return err;
   }


   ctx->comm.sequenceNumber = 0;

   g_free(ctx->comm.userName);
   ctx->comm.userName = NULL;

#ifdef _WIN32
   if (ctx->comm.hPipe != INVALID_HANDLE_VALUE) {
      CloseHandle(ctx->comm.hPipe);
      ctx->comm.hPipe = INVALID_HANDLE_VALUE;
   }
#else
   if (ctx->comm.sock >= 0) {
      close(ctx->comm.sock);
   }
#endif

   g_free(ctx->comm.pipeName);
   ctx->comm.pipeName = NULL;

#ifdef UNITTEST
   if (ctx->comm.fileTest) {
      fclose(ctx->comm.testFp);
   }
#endif

   ctx->comm.connected = FALSE;

   return err;
}


/*
 ******************************************************************************
 * VGAuth_ConnectToServiceAsUser --                                      */ /**
 *
 * Makes the connection to the public service, handles the initial
 * handshake, then connects to the user-specific pipe.
 *
 * @param[in]  ctx        The VGAuthContext.
 * @param[in]  userName   The user.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_ConnectToServiceAsUser(VGAuthContext *ctx,
                              const char *userName)
{
   VGAuthError err = VGAUTH_E_OK;
   gchar *pipeName = NULL;
   VGAuthContext *pubCtx = NULL;

   if (VGAuth_IsConnectedToServiceAsUser(ctx, userName)) {
      Debug("%s: already connected as '%s'\n", __FUNCTION__, userName);
      // treat this as a no-op
      goto done;
   }

   /*
    * If currently connected (presumably as another user), close down
    * and re-open.
    */

   VGAuth_CloseConnection(ctx);

   /*
    * Make a temp context to connect to the public pipe.
    */
   pubCtx = g_malloc0(sizeof(VGAuthContext));
   if (NULL == pubCtx) {
      return VGAUTH_E_OUT_OF_MEMORY;
   }

   pubCtx->comm.pipeName = g_strdup(SERVICE_PUBLIC_PIPE_NAME);
   pubCtx->comm.userName = g_strdup(SUPERUSER_NAME);
   err = VGAuth_InitConnection(pubCtx);
   if (err != VGAUTH_E_OK) {
      Warning("%s: Failed to init public pipe connection "VGAUTHERR_FMT64X"\n",
              __FUNCTION__, err);
      goto done;
   }

   err = VGAuth_NetworkConnect(pubCtx);
   if (err != VGAUTH_E_OK) {
      Warning("%s: Failed to connect to public pipe "VGAUTHERR_FMT64X"\n",
              __FUNCTION__, err);
      goto done;
   }


   /*
    * The public pipe should be owned by superUser, otherwise
    * we have a spoofer.
    */
   if (!VGAuth_NetworkValidatePublicPipeOwner(pubCtx)) {
      Warning("%s: security violation!  public pipe is not owned by super-user!\n",
              __FUNCTION__);
      err = VGAUTH_E_SECURITY_VIOLATION;
      goto done;
   }

   /*
    * SessionRequest will get back a user-specific pipeName.
    */
   err = VGAuth_SendSessionRequest(pubCtx, userName, &pipeName);
   if (err != VGAUTH_E_OK) {
      Warning("%s: Failed to initiate session "VGAUTHERR_FMT64X"\n",
              __FUNCTION__, err);
      goto done;
   }

   /*
    * Set up for the user pipe.
    */
   ctx->comm.userName = g_strdup(userName);
   ctx->comm.pipeName = g_strdup(pipeName);

   err = VGAuth_NetworkConnect(ctx);
   if (err != VGAUTH_E_OK) {
      Warning("%s: Failed to connect to private pipe "VGAUTHERR_FMT64X"\n",
              __FUNCTION__, err);
      goto done;
   }

   /*
    * Do initial handshake.
    */
   err = VGAuth_SendConnectRequest(ctx);
   if (err != VGAUTH_E_OK) {
      Warning("%s: Failed to connect user session "VGAUTHERR_FMT64X"\n",
              __FUNCTION__, err);
      goto done;
   }

   /*
    * The user-private connection is good to go.
    */

done:
   VGAuth_CloseConnection(pubCtx);
   g_free(pubCtx);

   g_free(pipeName);
   return err;
}


/*
 ******************************************************************************
 * VGAuth_ConnectToServiceAsCurrentUser --                               */ /**
 *
 * Makes the connection to the public service, handles the initial
 * handshake, then connects to the user-specific pipe.
 *
 * This is a wrapper on VGAuth_ConnectToServiceAsUser() using the
 * current user.  This is useful for requests like QueryMappedCerts
 * which can be done as any user; we know the current user will be able to
 * connect to its private pipe to the service.
 *
 * @param[in]  ctx        The VGAuthContext.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_ConnectToServiceAsCurrentUser(VGAuthContext *ctx)
{
   VGAuthError err;
   gchar *currentUsername;

   currentUsername = VGAuth_GetCurrentUsername();
   if (NULL == currentUsername) {
      return VGAUTH_E_FAIL;
   }

   err = VGAuth_ConnectToServiceAsUser(ctx, currentUsername);

   g_free(currentUsername);

   return err;
}


/*
 ******************************************************************************
 * VGAuth_CommSendData --                                                */ /**
 *
 * Sends a NUL-terminated string to the service.
 *
 * @param[in]  ctx        The VGAuthContext.
 * @param[in]  packet     The data to be sent.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_CommSendData(VGAuthContext *ctx,
                    gchar *packet)
{
   return VGAuth_NetworkWriteBytes(ctx, strlen(packet), packet);
}


/*
 ******************************************************************************
 * VGAuth_CommReadData --                                                */ /**
 *
 * Reads some data from the service.  This will just be the next chunk
 * read off the wire, and may not be a complete packet.
 *
 * @param[in]  ctx        The VGAuthContext.
 * @param[out] userName   The length of the data read.
 * @param[out] response   The data read.  Should be g_free()d by caller.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_CommReadData(VGAuthContext *ctx,
                    gsize *len,
                    gchar **response)
{
   VGAuthError err = VGAUTH_E_OK;
#ifdef UNITTEST
   if (ctx->comm.fileTest) {
      char buf[2];
      char *rBuf;

      ctx->comm.sequenceNumber = -1;
      /*
       * This is absurdly inefficient, but it lets me put a bunch
       * of test replies in a single file.
       */
      rBuf = fgets(buf, 2, ctx->comm.testFp);
      if (NULL == rBuf) {     // EOF
         *len = 0;
         err = VGAUTH_E_COMM;
         goto quit;
      }
      *len = 1;
      *response = g_strdup(buf);
   } else if (ctx->comm.bufTest) {
      // XXX can make this reply with chunks for extra testing
      if (ctx->comm.bufLoc == ctx->comm.bufLen) {
         *len = 0;
         err = VGAUTH_E_COMM;
         goto quit;
      }
      *response = g_strdup(ctx->comm.testBuffer);
      *len = ctx->comm.bufLen;
      ctx->comm.bufLoc = ctx->comm.bufLen;
   } else
#endif
   {
      err = VGAuth_NetworkReadBytes(ctx, len, response);
   }
#ifdef UNITTEST
quit:
#endif
   return err;
}


#ifdef UNITTEST
/*
 ******************************************************************************
 * VGAuthComm_SetTestFileInput --                                        */ /**
 *
 * Sets up the communication channel as a file for use in testing.
 *
 * @param[in]  ctx        The VGAuthContext.
 * @param[in]  filename   The file containing the test input.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuthComm_SetTestFileInput(VGAuthContext *ctx,
                            const char *filename)
{
   VGAuthError err = VGAUTH_E_OK;

   ctx->comm.testFp = g_fopen(filename, "r");
   if (NULL == ctx->comm.testFp) {
      fprintf(stderr, "Failed to open test input file '%s'\n", filename);
      err = VGAUTH_E_COMM;
   } else {
      ctx->comm.fileTest = TRUE;
   }
   return err;
}


/*
 ******************************************************************************
 * VGAuthComm_SetTestBufferInput --                                      */ /**
 *
 * Sets up the communication channel as a buffer for use in testing.
 *
 * @param[in]  ctx        The VGAuthContext.
 * @param[in]  buffer     The buffer containing the test input.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuthComm_SetTestBufferInput(VGAuthContext *ctx,
                              const char *buffer)
{
   VGAuthError err;
   size_t bufLen;

   ctx->comm.bufTest = TRUE;
   ctx->comm.bufLoc = 0;
   bufLen = strlen(buffer);

   if (bufLen > sizeof ctx->comm.testBuffer - 1) {
      fprintf(stderr, "Test buffer too large.\n");
      err = VGAUTH_E_INVALID_ARGUMENT;
   } else {
      ctx->comm.bufLen = bufLen;
      strncpy(ctx->comm.testBuffer, buffer, sizeof ctx->comm.testBuffer - 1);
      ctx->comm.testBuffer[sizeof ctx->comm.testBuffer - 1] = '\0';
      err = VGAUTH_E_OK;
   }

   return err;
}
#endif

