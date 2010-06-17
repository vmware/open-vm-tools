/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
   LogHandlerData    handler;
   GStaticMutex      lock;
   RpcChannel       *chan;
} VMXLoggerData;


/*
 *******************************************************************************
 * VMXLoggerLog --                                                        */ /**
 *
 * Logs a message to the VMX using the backdoor.
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
 * @param[in] _data     VMX logger data.
 * @param[in] errfn     Unused.
 *
 * @return TRUE if successfully logged the message.
 *
 *******************************************************************************
 */

static gboolean
VMXLoggerLog(const gchar *domain,
             GLogLevelFlags level,
             const gchar *message,
             LogHandlerData *_data,
             LogErrorFn errfn)
{
   gboolean ret = FALSE;
   VMXLoggerData *data = (VMXLoggerData *) _data;

   g_static_mutex_lock(&data->lock);
   if (RpcChannel_Start(data->chan)) {
      gchar *msg;
      gint cnt = VMToolsAsprintf(&msg, "log %s", message);

      /*
       * XXX: RpcChannel_Send() can log stuff in certain situations, which will
       * cause this to blow up. Hopefully we won't hit those too often since
       * we're stopping / starting the channel for each log message.
       */
      ret = RpcChannel_Send(data->chan, msg, cnt, NULL, NULL);

      g_free(msg);
      RpcChannel_Stop(data->chan);
   }
   g_static_mutex_unlock(&data->lock);

   return ret;
}


/*
 *******************************************************************************
 * VMXLoggerDestroy --                                                    */ /**
 *
 * Cleans up the internal state of a VMX logger.
 *
 * @param[in] _data     VMX logger data.
 *
 *******************************************************************************
 */

static void
VMXLoggerDestroy(LogHandlerData *_data)
{
   VMXLoggerData *data = (VMXLoggerData *) _data;
   RpcChannel_Destroy(data->chan);
   g_static_mutex_free(&data->lock);
   g_free(data);
}


/*
 *******************************************************************************
 * VMXLoggerConfig --                                                     */ /**
 *
 * Configures a new VMX logger.
 *
 * @param[in] defaultDomain   Unused.
 * @param[in] domain          Unused.
 * @param[in] name            Unused.
 * @param[in] cfg             Unused.
 *
 * @return The VMX logger data.
 *
 *******************************************************************************
 */

LogHandlerData *
VMXLoggerConfig(const gchar *defaultDomain,
                const gchar *domain,
                const gchar *name,
                GKeyFile *cfg)
{
   VMXLoggerData *data = g_new0(VMXLoggerData, 1);
   data->handler.logfn = VMXLoggerLog;
   data->handler.convertToLocal = FALSE;
   data->handler.timestamp = FALSE;
   data->handler.shared = TRUE;
   data->handler.copyfn = NULL;
   data->handler.dtor = VMXLoggerDestroy;
   g_static_mutex_init(&data->lock);
   data->chan = BackdoorChannel_New();
   return &data->handler;
}

