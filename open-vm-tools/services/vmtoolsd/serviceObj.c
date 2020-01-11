/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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


typedef struct ServiceProperty {
   guint       id;
   gchar      *name;
   gpointer    value;
} ServiceProperty;

static gpointer gToolsCoreServiceParentClass;

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


#if defined(_WIN32)
/**
 * Accumulator function for the "service control" signal. Updates the return
 * value according to the signal's documentation.
 *
 * The gobject library initializes the return value to "0" regardless of
 * what the signal emitter sets it to. So the accumulator does two things
 * to have a non-zero default return value:
 *
 *    - if the current return value is zero, it's set to the default return
 *      value (ERROR_CALL_NOT_IMPLEMENTED).
 *    - the return value is always offset by one; so the signal emitter
 *      should decrement the return value when looking at it.
 *
 * @param[in]     ihint       Unused.
 * @param[in,out] retval      Return value of the signal (offset by 1).
 * @param[in]     handlerRet  Return value from the current handler.
 * @param[in]     data        Unused.
 *
 * @return TRUE.
 */

static gboolean
ToolsCoreServiceControlAccumulator(GSignalInvocationHint *ihint,
                                   GValue *retval,
                                   const GValue *handlerRet,
                                   gpointer data)
{
   guint ret = g_value_get_uint(retval);
   guint handlerVal = g_value_get_uint(handlerRet);

   if (ret == 0) {
      ret = ERROR_CALL_NOT_IMPLEMENTED + 1;
   }

   switch (ret) {
   case ERROR_CALL_NOT_IMPLEMENTED + 1:
      ret = handlerVal + 1;
      break;

   case NO_ERROR + 1:
      if (handlerVal != ERROR_CALL_NOT_IMPLEMENTED) {
         ret = handlerVal + 1;
      }
      break;

   default:
      break;
   }

   g_value_set_uint(retval, ret);
   return TRUE;
}
#endif


/*
 *******************************************************************************
 * ToolsCoreServiceGetProperty --                                         */ /**
 *
 * Gets the value of a property in the object.
 *
 * @param[in]  object   The instance.
 * @param[in]  id       Property ID.
 * @param[out] value    Where to set the value.
 * @param[in]  pspec    Unused.
 *
 *******************************************************************************
 */

static void
ToolsCoreServiceGetProperty(GObject *object,
                            guint id,
                            GValue *value,
                            GParamSpec *pspec)
{
   ToolsCoreService *self = (ToolsCoreService *) object;

   id -= 1;

   g_mutex_lock(&self->lock);

   if (id < self->props->len) {
      ServiceProperty *p = &g_array_index(self->props, ServiceProperty, id);
      g_value_set_pointer(value, p->value);
   }

   g_mutex_unlock(&self->lock);
}


/*
 *******************************************************************************
 * ToolsCoreServiceSetProperty --                                         */ /**
 *
 * Sets a property in the given object. If the property is found, a "notify"
 * signal is sent so that interested listeners can act on the change.
 *
 * @param[in] object The instance.
 * @param[in] id     Property ID.
 * @param[in] value  Value to set.
 * @param[in] pspec  Unused.
 *
 *******************************************************************************
 */

static void
ToolsCoreServiceSetProperty(GObject *object,
                            guint id,
                            const GValue *value,
                            GParamSpec *pspec)
{
   ServiceProperty *p = NULL;
   ToolsCoreService *self = (ToolsCoreService *) object;

   id -= 1;

   g_mutex_lock(&self->lock);

   if (id < self->props->len) {
      p = &g_array_index(self->props, ServiceProperty, id);
      p->value = g_value_get_pointer(value);
   }

   g_mutex_unlock(&self->lock);

   if (p != NULL) {
      g_object_notify(object, p->name);
   }
}


/*
 *******************************************************************************
 * ToolsCoreServiceCtor --                                                */ /**
 *
 * Object constructor. Initialize internal state.
 *
 * @param[in] type      Object type.
 * @param[in] nparams   Param count.
 * @param[in] params    Construction parameters.
 *
 * @return A new instance.
 *
 *******************************************************************************
 */

static GObject *
ToolsCoreServiceCtor(GType type,
                     guint nparams,
                     GObjectConstructParam *params)
{
   GObject *object;
   ToolsCoreService *self;

   object = G_OBJECT_CLASS(gToolsCoreServiceParentClass)->constructor(type,
                                                                      nparams,
                                                                      params);

   self = TOOLSCORE_SERVICE(object);
   g_mutex_init(&self->lock);
   self->props = g_array_new(FALSE, FALSE, sizeof (ServiceProperty));

   return object;
}


/*
 *******************************************************************************
 * ToolsCoreServiceDtor --                                                */ /**
 *
 * Object destructor. Frees memory associated with the object. Goes through the
 * list of properties to make sure all of them have been cleaned up before the
 * service exits, printing a warning otherwise.
 *
 * @param[in]  object   The object being destructed.
 *
 *******************************************************************************
 */

static void
ToolsCoreServiceDtor(GObject *object)
{
   ToolsCoreService *self = (ToolsCoreService *) object;
   guint i;

   for (i = 0; i < self->props->len; i++) {
      ServiceProperty *p = &g_array_index(self->props, ServiceProperty, i);
      if (p->value != NULL) {
         g_warning("Property '%s' was not cleaned up before shut down.",
                   p->name);
      }
      g_free(p->name);
   }

   g_array_free(self->props, TRUE);
   g_mutex_clear(&self->lock);
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

   gToolsCoreServiceParentClass = g_type_class_peek_parent(_klass);

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
   g_signal_new(TOOLS_CORE_SIG_CONF_RELOAD,
                G_OBJECT_CLASS_TYPE(klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__POINTER,
                G_TYPE_NONE,
                1,
                G_TYPE_POINTER);
   g_signal_new(TOOLS_CORE_SIG_DUMP_STATE,
                G_OBJECT_CLASS_TYPE(klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__POINTER,
                G_TYPE_NONE,
                1,
                G_TYPE_POINTER);
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
   g_signal_new(TOOLS_CORE_SIG_NO_RPC,
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
   g_signal_new(TOOLS_CORE_SIG_SERVICE_CONTROL,
                G_OBJECT_CLASS_TYPE(klass),
                G_SIGNAL_RUN_LAST,
                0,
                ToolsCoreServiceControlAccumulator,
                NULL,
                g_cclosure_user_marshal_UINT__POINTER_POINTER_UINT_UINT_POINTER,
                G_TYPE_UINT,
                5,
                G_TYPE_POINTER,
                G_TYPE_POINTER,
                G_TYPE_UINT,
                G_TYPE_UINT,
                G_TYPE_POINTER);
#endif

   G_OBJECT_CLASS(klass)->constructor = ToolsCoreServiceCtor;
   G_OBJECT_CLASS(klass)->finalize = ToolsCoreServiceDtor;
   G_OBJECT_CLASS(klass)->set_property = ToolsCoreServiceSetProperty;
   G_OBJECT_CLASS(klass)->get_property = ToolsCoreServiceGetProperty;
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


/*
 *******************************************************************************
 * ToolsCoreService_RegisterProperty --                                   */ /**
 *
 * Installs a new property in the service object.
 *
 * @param[in] obj    Service object.
 * @param[in] prop   Property to install.
 *
 *******************************************************************************
 */

void
ToolsCoreService_RegisterProperty(ToolsCoreService *obj,
                                  ToolsServiceProperty *prop)
{
   static guint PROP_ID_SEQ = 0;

   ServiceProperty sprop;
   ToolsCoreServiceClass *klass = TOOLSCORESERVICE_GET_CLASS(obj);
   GParamSpec *pspec = g_param_spec_pointer(prop->name,
                                            prop->name,
                                            prop->name,
                                            G_PARAM_READWRITE);

   g_mutex_lock(&obj->lock);

   sprop.id = ++PROP_ID_SEQ;
   sprop.name = g_strdup(prop->name);
   sprop.value = NULL;
   g_array_append_val(obj->props, sprop);
   g_object_class_install_property(G_OBJECT_CLASS(klass), sprop.id, pspec);

   g_mutex_unlock(&obj->lock);
}

