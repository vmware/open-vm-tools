/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
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

/**
 * @file vmxLogger.c
 *
 * A logger that writes the logs to the VMX log file.
 */

#include "vmtoolsInt.h"
#include "vmware/tools/guestrpc.h"

typedef struct VMXLoggerData {
   GlibLogger     handler;
   RpcChannel    *chan;
} VMXLoggerData;


/*
 *******************************************************************************
 * VMXLoggerLog --                                                        */ /**
 *
 * Logs a message to the VMX using RpcChannel.
 *
 * The logger uses its own RpcChannel, opening and closing the channel for each
 * log message sent. This is not optimal, especially if the application already
 * has an RpcChannel instantiated; this could be fixed by providing a way for
 * the application to provide its own RpcChannel to the logging code, if it uses
 * one, so that this logger can re-use it.
 *
 * @param[in] domain    Unused.
 * @param[in] level     Log level.
 * @param[in] message   Message to log.
 * @param[in] data      VMX logger data.
 *
 *******************************************************************************
 */

static void
VMXLoggerLog(const gchar *domain,
             GLogLevelFlags level,
             const gchar *message,
             gpointer data)
{
   VMXLoggerData *logger = data;

   if (RpcChannel_Start(logger->chan)) {
      gchar *msg;
      gint cnt = VMToolsAsprintf(&msg, "log %s", message);

      RpcChannel_Send(logger->chan, msg, cnt, NULL, NULL);

      g_free(msg);
      RpcChannel_Stop(logger->chan);
   }
}


/*
 *******************************************************************************
 * VMXLoggerDestroy --                                                    */ /**
 *
 * Cleans up the internal state of a VMX logger.
 *
 * @param[in] data   VMX logger data.
 *
 *******************************************************************************
 */

static void
VMXLoggerDestroy(gpointer data)
{
   VMXLoggerData *logger = data;
   RpcChannel_Destroy(logger->chan);
   g_free(logger);
}


/*
 *******************************************************************************
 * VMToolsCreateVMXLogger --                                              */ /**
 *
 * Configures a new VMX logger.
 *
 * @return The VMX logger data.
 *
 *******************************************************************************
 */

GlibLogger *
VMToolsCreateVMXLogger(void)
{
   VMXLoggerData *data = g_new0(VMXLoggerData, 1);
   data->handler.logfn = VMXLoggerLog;
   data->handler.addsTimestamp = TRUE;
   data->handler.shared = TRUE;
   data->handler.dtor = VMXLoggerDestroy;
   data->chan = RpcChannel_New();
   return &data->handler;
}

