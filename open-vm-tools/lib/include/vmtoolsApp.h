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

#ifndef _VMTOOLSAPP_H_
#define _VMTOOLSAPP_H_

/**
 * @file vmtoolsApp.h
 *
 *    Defines the interface between the core tools services and the plugins
 *    that are dynamically loaded into the service.
 *
 * @addtogroup vmtools_plugins
 * @{
 */

#include <glib.h>
#if defined(G_PLATFORM_WIN32)
#  include <windows.h>
#  include <objbase.h>
#endif
#include "rpcChannel.h"
#include "vmware/guestrpc/capabilities.h"

/**
 * Error reporting macro. Call this if the app encounters an error
 * that requires the service to quit. The service's main loop will stop
 * as soon as it regains control of the application.
 *
 * @param[in]  ctx      The application context.
 * @param[in]  err      Error code. Must not be 0.
 */
#define VMTOOLSAPP_ERROR(ctx, err) do {   \
   ASSERT((err) != 0);                    \
   (ctx)->errorCode = (err);              \
   g_main_loop_quit((ctx)->mainLoop);     \
} while (0)


/**
 * Attaches the given event source to the app context's main loop.
 *
 * @param[in]  ctx      The application context.
 * @param[in]  src      Source to attach.
 * @param[in]  cb       Callback to call when event is "ready".
 * @param[in]  data     Data to provide to the callback.
 * @param[in]  destroy  Destruction notification callback.
 */
#define VMTOOLSAPP_ATTACH_SOURCE(ctx, src, cb, data, destroy) do {      \
   GSource *__src = (src);                                              \
   g_source_set_callback(__src, (GSourceFunc) (cb), (data), (destroy)); \
   g_source_attach(__src, g_main_loop_get_context((ctx)->mainLoop));    \
} while (0)


/**
 * Signal sent when registering or unregistering capabilities.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      ToolsAppCtx *: the application context.
 * @param[in]  set      gboolean: TRUE if setting capabilities, FALSE if unsetting them.
 * @param[in]  data     Client data.
 *
 * @return A GArray instance with the capabilities to be set or unset. The
 *         elements should be of type ToolsAppCapability.
 */
#define TOOLS_CORE_SIG_CAPABILITIES "tcs_capabilities"

/**
 * Signal sent when the config file is reloaded.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      ToolsAppCtx *: The application context.
 * @param[in]  data     Client data.
 */
#define TOOLS_CORE_SIG_CONF_RELOAD "tcs_conf_reload"

/**
 * Signal sent when the service receives a request to dump its internal
 * state to the log. This is for debugging purposes, and plugins can
 * respond to the signal by dumping their own state also.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      ToolsAppCtx *: The application context.
 * @param[in]  data     Client data.
 */
#define TOOLS_CORE_SIG_DUMP_STATE  "tcs_dump_state"

/**
 * Signal sent when a successful RpcChannel reset occurs.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      ToolsAppCtx *: The application context.
 * @param[in]  data     Client data.
 */
#define TOOLS_CORE_SIG_RESET  "tcs_reset"

/**
 * Signal sent when a "set option" RPC message arrives.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      ToolsAppCtx *: The application context.
 * @param[in]  option   gchar *: Option being set.
 * @param[in]  value    gchar *: Option value.
 * @param[in]  data     Client data.
 *
 * @return A gboolean saying whether the option was recognized and the value
 *         was valid.
 */
#define TOOLS_CORE_SIG_SET_OPTION "tcs_set_option"

/**
 * Signal sent when shutting down the service.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      ToolsAppCtx *: The application context.
 * @param[in]  data     Client data.
 */
#define TOOLS_CORE_SIG_SHUTDOWN "tcs_shutdown"

#if defined(G_PLATFORM_WIN32)
/**
 * Signal sent when there's a change in the state of a user's session.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      ToolsAppCtx *: The application context.
 * @param[in]  code     DWORD: Session state change code.
 * @param[in]  id       DWORD: Session ID.
 * @param[in]  data     Client data.
 */
#define TOOLS_CORE_SIG_SESSION_CHANGE  "tcs_session_change"
#endif


/**
 * This enum lists all API versions that different versions of vmtoolsd support.
 * The "ToolsAppCtx" instance provided to plugins contains a "version" field
 * which is a bit-mask of these values, telling plugins what features the
 * container supports.
 *
 * Refer to a specific feature's documentation for which version of the API
 * is needed for it to be supported.
 */
typedef enum {
   TOOLS_CORE_API_V1    = 0x1,
} ToolsCoreAPI;


/**
 * Defines the context of a tools application. This data is provided by the
 * core services to applications when they're loaded.
 */
typedef struct ToolsAppCtx {
   /** Supported API versions. This is a bit-mask. */
   ToolsCoreAPI      version;
   /** Name of the application. */
   const gchar      *name;
   /** Whether we're running under a VMware hypervisor. */
   gboolean          isVMware;
   /** Error code to return from the main loop. */
   int               errorCode;
   /** The main loop instance for the service. */
   GMainLoop        *mainLoop;
   /** The RPC channel used to communicate with the VMX. */
   RpcChannel       *rpc;
   /** Service configuration from the config file. */
   GKeyFile         *config;
#if defined(G_PLATFORM_WIN32)
   /** Whether COM is initialized. */
   gboolean          comInitialized;
#else
   /** The FD to access the VMware blocking fs. -1 if no FD available. */
   int               blockFD;
   /** The native environment (without any VMware modifications). */
   const char      **envp;
#endif
   /**
    * A GObject instance shared among all plugins. The object itself doesn't
    * provide any functionality; but the service emits a few signals on this
    * object (see the signal name declarations in this header), and plugins can
    * register and emit their own signals using this object.
    */
   gpointer          serviceObj;
} ToolsAppCtx;

#if defined(G_PLATFORM_WIN32)
/**
 * Initializes COM if it hasn't been initialized yet.
 *
 * @param[in]  ctx   The application context.
 *
 * @return TRUE if COM is initialized when the function returns.
 */
static INLINE gboolean
ToolsCore_InitializeCOM(ToolsAppCtx *ctx)
{
   if (!ctx->comInitialized) {
      HRESULT ret = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
      ctx->comInitialized = SUCCEEDED(ret);
      if (!ctx->comInitialized) {
         g_log(ctx->name, G_LOG_LEVEL_WARNING,
               "COM initialization failed(0x%x)\n", ret);
      }
   }
   return ctx->comInitialized;
}
#endif


/* Capabilities. */

/** Identifies the type of a Tools capability. */
typedef enum {
   TOOLS_CAP_OLD        = 0,
   TOOLS_CAP_OLD_NOVAL  = 1,
   TOOLS_CAP_NEW        = 2
} ToolsCapabilityType;

/**
 * Information about a capability supported by the application. This structure
 * supports both old-style capabilities (which have a separate RPC message for
 * each capability) and new-style capabilities (as defined in guestCaps.h).
 *
 * The service will register all capabilities with non-zero values when the
 * service is started (or the host asks for the service to register its
 * capabilities).
 */
typedef struct ToolsAppCapability {
   /** Identifies the type of the capability. */
   ToolsCapabilityType  type;
   /**
    * For old-style, the capability name. The RPC message for setting the
    * capability will be "tools.capability.[name]". Ignored for TOOLS_CAP_NEW.
    */
   gchar               *name;
   /**
    * The capability entry in the enum defined in guestCaps.h.
    * Used only for TOOLS_CAP_NEW.
    */
   GuestCapabilities    index;
   /** The capability value. 0 means disabled. Ignored for TOOLS_CAP_OLD_NOVAL. */
   guint                value;
} ToolsAppCapability;


/* Application registration. */

/** Type of the application feature being registered. */
typedef enum {
   /**
    * Denotes a list of GuestRPC registrations (type RpcChannelCallback).
    */
   TOOLS_APP_GUESTRPC   = 1,
   /**
    * Denotes a list of signals the application is interested in (type
    * ToolsPluginSignalCb).
    */
   TOOLS_APP_SIGNALS    = 2,
   /**
    * Denotes an application provider (type ToolsAppProvider). This allows
    * plugins to extend the functionality of vmtoolsd by adding new application
    * types (that other plugins can hook into).
    */
   TOOLS_APP_PROVIDER   = 3,
} ToolsAppType;


/**
 * Defines the registration data for an "application provider". Application
 * providers allow plugins to hook into new application frameworks that will
 * be then managed by vmtoolsd - for example, an HTTP server or a dbus endpoint.
 *
 * Application providers will be loaded during startup but not activated until
 * at least one plugin provides registration data for that provider.
 */
typedef struct ToolsAppProvider {
   /** A name describing the provider. */
   const gchar   *name;
   /**
    * Application type. Optimally, new providers would request a new type to be
    * added to the "official" ToolsAppType enum declared above, although that
    * is not strictly necessary. Providers should at least try to choose an
    * unused value.
    */
   ToolsAppType   regType;
   /** Size of the registration structure for this provider. */
   size_t         regSize;
   /**
    * Activation callback (optional). This is called when vmtoolsd detects that
    * there is at least one application that needs to be registered with this
    * provider.
    *
    * @param[in]  ctx   The application context.
    * @param[in]  prov  The provider instance.
    * @param[out] err   Where to store any error information.
    */
   void (*activate)(ToolsAppCtx *ctx, struct ToolsAppProvider *prov, GError **err);
   /**
    * Registration callback. This is called after "activate", to register an
    * application provided by a plugin.
    *
    * @param[in]  ctx   The application context.
    * @param[in]  prov  The provider instance.
    * @param[in]  reg   The application registration data.
    */
   void (*registerApp)(ToolsAppCtx *ctx, struct ToolsAppProvider *prov, gpointer reg);
   /**
    * Shutdown callback (optional). Called when the service is being shut down.
    * The provider is responsible for keeping track of registrations and
    * cleaning them up during shutdown.
    *
    * @param[in]  ctx   The application context.
    * @param[in]  prov  The provider instance.
    */
   void (*shutdown)(ToolsAppCtx *ctx, struct ToolsAppProvider *prov);
   /**
    * Debugging callback (optional). This callback is called when dumping the
    * service state to the logs for debugging purposes.
    *
    * This callback is called once with a "NULL" reg, so that the provider can
    * log its internal state, and then once for each registration struct
    * provided by loaded plugins.
    *
    * @param[in]  ctx   The application context.
    * @param[in]  prov  The provider instance.
    * @param[in]  reg   The application registration data.
    */
   void (*dumpState)(ToolsAppCtx *ctx, struct ToolsAppProvider *prov, gpointer reg);
} ToolsAppProvider;


/**
 * Defines a "transport-specific" registration. The array contains data specific
 * to a "transport" implementation.
 *
 * When the service is shutting down, if the @a data field is not NULL, the
 * array instance will be freed, including its backing element array.
 * See the documentation for g_array_free(). This will happen only after any
 * plugin's shutdown callback is called, so plugins have a chance of performing
 * custom clean up of this data.
 */
typedef struct ToolsAppReg {
   ToolsAppType   type;
   GArray        *data;
} ToolsAppReg;


/**
 * Defines a struct for mapping callbacks to signals. Normally it would suffice
 * to use g_signal_connect() directly to register interest in signals; but to
 * allow dynamic registration of signals by plugins, using this struct allows
 * registration to be delayed until all plugins have been loaded and have had
 * the chance to register their own signals. The daemon code then can go
 * through the plugins' registration data and connect all desired signals.
 */

typedef struct ToolsPluginSignalCb {
   const gchar   *signame;
   gpointer       callback;
   gpointer       clientData;
} ToolsPluginSignalCb;


/**
 * The registration data for an application. This gives the service information
 * about all functionality exported by the application, and any events that the
 * application may be interested in.
 *
 * When the plugin is shut down, if the @a regs field is not NULL, it (and its
 * element array) are freed with g_array_free().
 */
typedef struct ToolsPluginData {
   /** Name of the application (required). */
   char                      *name;
   /** List of features provided by the app. */
   GArray                    *regs;
   /** Private plugin data. */
   gpointer                   _private;
} ToolsPluginData;

/**
 * Definition for tagging functions to be exported in the plugin binary. Use this
 * to tag the plugin entry point function, and any other functions that the plugin
 * needs to export.
 */
#if defined(G_PLATFORM_WIN32)
#  define TOOLS_MODULE_EXPORT    __declspec(dllexport)
#elif defined(GCC_EXPLICIT_EXPORT)
#  define TOOLS_MODULE_EXPORT    __attribute__((visibility("default")))
#else
#  define TOOLS_MODULE_EXPORT
#endif

/**
 * Signature for the plugin entry point function. The function should be called
 * @a ToolsOnLoad, and be exported in the plugin binary (e.g., by tagging it
 * with TOOLS_MODULE_EXPORT).
 *
 * If the plugin wants to stay loaded, it always should return the registration
 * data, even if all it contains is the (mandatory) plugin name. Plugins which
 * return NULL will be unloaded before the service is started, so they shouldn't
 * modify the service state (for example, by adding callbacks to the service's
 * main loop).
 */
typedef ToolsPluginData *(*ToolsPluginOnLoad)(ToolsAppCtx *ctx);

/** @} */

#endif /* _VMTOOLSAPP_H_ */

