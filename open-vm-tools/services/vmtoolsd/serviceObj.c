/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * @file serviceObj.c
 *
 * Implementation of the "ToolsCore_Service" gobject.
 */

#include "toolsCoreInt.h"
#include "serviceObj.h"
#include "svcSignals.h"
#include "vmtoolsApp.h"


/**
 * Accumulator function for the "set option" signal. If a handler returns
 * TRUE, sets the result of the signal propagation to TRUE.
 *
 * @param[in]  ihint       Unused.
 * @param[out] retval      Return value of the signal.
 * @param[in]  handlerRet  Return value from the current handler.
 * @param[in]  data        Unused.
 *
 * @return TRUE
 */

static gboolean
ToolsCoreSetOptionAccumulator(GSignalInvocationHint *ihint,
                              GValue *retval,
                              const GValue *handlerRet,
                              gpointer data)
{
   if (!g_value_get_boolean(retval)) {
      g_value_set_boolean(retval, g_value_get_boolean(handlerRet));
   }
   return TRUE;
}


/**
 * Accumulator function for the "capabilities" signal. Just puts the contents
 * of all returned GArray instances into a "master" instance.
 *
 * @param[in]  ihint       Unused.
 * @param[out] retval      Return value of the signal.
 * @param[in]  handlerRet  Return value from the current handler.
 * @param[in]  data        Unused.
 *
 * @return TRUE.
 */

static gboolean
ToolsCoreCapabilitiesAccumulator(GSignalInvocationHint *ihint,
                                 GValue *retval,
                                 const GValue *handlerRet,
                                 gpointer data)
{
   GArray *caps = g_value_get_pointer(handlerRet);

   if (caps != NULL) {
      guint i;
      GArray *acc = g_value_get_pointer(retval);

      if (acc == NULL) {
         acc = g_array_new(FALSE, TRUE, sizeof (ToolsAppCapability));
         g_value_set_pointer(retval, acc);
      }

      for (i = 0; i < caps->len; i++) {
         g_array_append_val(acc, g_array_index(caps, ToolsAppCapability, i));
      }

      g_array_free(caps, TRUE);
   }

   return TRUE;
}


/**
 * Initializes the ToolsCoreService class. Sets up the signals that are sent
 * by the vmtoolsd service.
 *
 * @param[in]  klass    The class instance to initialize.
 */

static void
ToolsCore_Service_class_init(gpointer _klass,
                             gpointer klassData)
{
   ToolsCoreServiceClass *klass = _klass;
   g_signal_new(TOOLS_CORE_SIG_CAPABILITIES,
                G_OBJECT_CLASS_TYPE(klass),
                G_SIGNAL_RUN_LAST,
                0,
                ToolsCoreCapabilitiesAccumulator,
                NULL,
                g_cclosure_user_marshal_POINTER__POINTER_BOOLEAN,
                G_TYPE_POINTER,
                2,
                G_TYPE_POINTER,
                G_TYPE_BOOLEAN);
   g_signal_new(TOOLS_CORE_SIG_RESET,
                G_OBJECT_CLASS_TYPE(klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__POINTER,
                G_TYPE_NONE,
                1,
                G_TYPE_POINTER);
   g_signal_new(TOOLS_CORE_SIG_SET_OPTION,
                G_OBJECT_CLASS_TYPE(klass),
                G_SIGNAL_RUN_LAST,
                0,
                ToolsCoreSetOptionAccumulator,
                NULL,
                g_cclosure_user_marshal_BOOLEAN__POINTER_STRING_STRING,
                G_TYPE_BOOLEAN,
                3,
                G_TYPE_POINTER,
                G_TYPE_STRING,
                G_TYPE_STRING);
   g_signal_new(TOOLS_CORE_SIG_SHUTDOWN,
                G_OBJECT_CLASS_TYPE(klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__POINTER,
                G_TYPE_NONE,
                1,
                G_TYPE_POINTER);
#if defined(G_PLATFORM_WIN32)
   g_signal_new(TOOLS_CORE_SIG_SESSION_CHANGE,
                G_OBJECT_CLASS_TYPE(klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL,
                NULL,
                g_cclosure_user_marshal_VOID__POINTER_UINT_UINT,
                G_TYPE_NONE,
                3,
                G_TYPE_POINTER,
                G_TYPE_UINT,
                G_TYPE_UINT);
#endif
}


/**
 * Initializes the ToolsCoreService type if it hasn't been done yet, and
 * return the type instance. This method is not thread safe.
 *
 * @return The ToolsCoreService type.
 */

GType
ToolsCore_Service_get_type(void)
{
   static GType type = 0;
   if (type == 0) {
      static const GTypeInfo info = {
         sizeof (ToolsCoreServiceClass),
         NULL,                               /* base_init */
         NULL,                               /* base_finalize */
         ToolsCore_Service_class_init,
         NULL,                               /* class_finalize */
         NULL,                               /* class_data */
         sizeof (ToolsCoreService),
         0,                                  /* n_preallocs */
         NULL,                               /* instance_init */
      };
      type = g_type_register_static(G_TYPE_OBJECT,
                                    "ToolsCoreService",
                                    &info,
                                    0);
   }
   return type;
}

