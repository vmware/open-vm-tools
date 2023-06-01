/*********************************************************
 * Copyright (c) 2020-2022 VMware, Inc. All rights reserved.
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
 * @file globalConfig.c
 *
 *    Implementation of the module that downloads the configuration from the
 *    GuestStore.
 */

/* Need this header in the beginning for GLOBALCONFIG_SUPPORTED definition. */
#include "globalConfig.h"

#if !defined(GLOBALCONFIG_SUPPORTED)
#   error This file should not be compiled
#endif

#include <errno.h>
#include "conf.h"
#include "file.h"
#include "guestApp.h"
#include <string.h>
#include <glib/gstdio.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include "str.h"
#include "vmware/tools/guestStore.h"
#include "vmware/tools/utils.h"
#include "vmware/tools/threadPool.h"
#include "guestStoreClient.h"

/**
 * Default value for CONFNAME_GLOBALCONF_ENABLED setting in tools.conf.
 *
 * TRUE will enable the module. GlobalConf module is disabled by default.
 */
#define GLOBALCONF_DEFAULT_ENABLED FALSE

/**
 * Default value for CONFNAME_GLOBALCONF_POLL_INTERVAL setting in tools.conf.
 */
#define GLOBALCONF_DEFAULT_POLL_INTERVAL (60 * 60)

/**
 * Minimum poll interval for fetching the global configuration from GuestStore.
 */
#ifdef VMX86_DEBUG
#define GLOBALCONF_MIN_POLL_INTERVAL (2 * 60)
#else
#define GLOBALCONF_MIN_POLL_INTERVAL (30 * 60)
#endif

/**
 * Default value for CONFNAME_GLOBALCONF_RESOURCE setting in tools.conf.
 */
#if defined(_WIN32)
#define GLOBALCONF_DEFAULT_RESOURCE \
        "/vmware/configurations/vmtools/windows/tools.conf"
#else
#define GLOBALCONF_DEFAULT_RESOURCE \
        "/vmware/configurations/vmtools/linux/tools.conf"
#endif

/**
 * Name of the local file populated with the global tools configuration.
 */
#define GLOBALCONF_LOCAL_FILENAME "tools-global.conf"

/**
 * Name of the local temp file populated with the global tools configuration.
 */
#define GLOBALCONF_LOCAL_TEMP_FILENAME "temp-global.conf"

/**
 * Macro to get the resource path from the config dictionary.
 */
#define GLOBALCONF_GET_RESOURCE_PATH(cfg) \
           VMTools_ConfigGetString(cfg, CONFGROUPNAME_GLOBALCONF, \
                                   CONFNAME_GLOBALCONF_RESOURCE, \
                                   GLOBALCONF_DEFAULT_RESOURCE)


typedef struct GlobalConfigInfo {
   gchar *localConfPath;
   gchar *localTempPath;
   gchar *guestStoreResource;
   guint pollInterval;
} GlobalConfigInfo;

typedef struct GlobalConfigThreadState {
   /*
    * 'mutex' protect concurrent accesses to terminate, cond, guestStoreEnabled
    * and useRandomInterval.
    */
   GMutex mutex;
   GCond cond;
   gboolean terminate;
   gboolean guestStoreEnabled;
   gboolean useRandomInterval;
   GlobalConfigInfo *configInfo;
} GlobalConfigThreadState;

static GlobalConfigThreadState globalConfigThreadState;


/*
 **************************************************************************
 * GuestStoreStateChanged --                                         */ /**
 *
 * GuestStore access state has changed, now update the download thread
 * accordingly.
 *
 * @param[in] src               Unused
 * @param[in] guestStoreEnabled GuestStore state enabled if TRUE
 * @param[in] unused            Unused
 *
 **************************************************************************
 */

static void
GuestStoreStateChanged(gpointer src,
                       gboolean guestStoreEnabled,
                       gpointer unused)
{
   GlobalConfigThreadState *threadState = &globalConfigThreadState;

   g_mutex_lock(&threadState->mutex);

   g_debug("%s: GuestStore old state: %d and new state: %d\n",
           __FUNCTION__, threadState->guestStoreEnabled, guestStoreEnabled);

   if (threadState->guestStoreEnabled != guestStoreEnabled) {
      threadState->guestStoreEnabled = guestStoreEnabled;
      g_debug("%s: Signalling the change in the GuestStore state.\n",
              __FUNCTION__);
      g_cond_signal(&threadState->cond);
   }

   g_mutex_unlock(&threadState->mutex);
}


/*
 **************************************************************************
 * GlobalConfigGetPollInterval --                                    */ /**
 *
 * Parses the configuration and returns the poll-interval.
 *
 * @param[in] cfg   VMTools configuration dictionary.
 *
 * @return unsigned integer representing the poll interval.
 *         0 if the globalconfig module is disabled.
 *         GLOBALCONF_DEFAULT_POLL_INTERVAL if an invalid value is specified.
 *
 **************************************************************************
 */

static guint
GlobalConfigGetPollInterval(GKeyFile *cfg)
{
   gint pollInterval = 0;
   gboolean enabled = GlobalConfig_GetEnabled(cfg);

   if (!enabled) {
      g_info("%s: global config module is disabled.", __FUNCTION__);
      return pollInterval;
   }

   pollInterval = VMTools_ConfigGetInteger(cfg, CONFGROUPNAME_GLOBALCONF,
                                           CONFNAME_GLOBALCONF_POLL_INTERVAL,
                                           GLOBALCONF_DEFAULT_POLL_INTERVAL);
   if (pollInterval < GLOBALCONF_MIN_POLL_INTERVAL) {
      g_warning("%s: Invalid value %d specified for '%s'. Using default %us",
                __FUNCTION__, pollInterval, CONFNAME_GLOBALCONF_POLL_INTERVAL,
                GLOBALCONF_DEFAULT_POLL_INTERVAL);
      pollInterval = GLOBALCONF_DEFAULT_POLL_INTERVAL;
   }

   return pollInterval;
}


/*
 **************************************************************************
 * GenerateRandomInterval --                                         */ /**
 *
 * Generate a random value between MIN_RAND_WAIT_INTERVAL and
 * MAX_RAND_WAIT_INTERVAL.
 *
 * @return a random generated unsigned integer value.
 *
 **************************************************************************
 */

static guint
GenerateRandomInterval(void)
{
   GRand *gRand = g_rand_new();

   /*
    * The following min and max values are taken randomly.
    */
#define MIN_RAND_WAIT_INTERVAL 30
#define MAX_RAND_WAIT_INTERVAL 300

   guint randomInterval = g_rand_int_range(gRand,
                                           MIN_RAND_WAIT_INTERVAL,
                                           MAX_RAND_WAIT_INTERVAL);
   g_rand_free(gRand);

#undef MIN_RAND_WAIT_INTERVAL
#undef MAX_RAND_WAIT_INTERVAL

   g_info("%s: Using random interval: %u.\n", __FUNCTION__, randomInterval);

   return randomInterval;
}


/*
 **************************************************************************
 * VMToolsChannelReset --                                            */ /**
 *
 * Callback function that gets called when the VMTools channel gets reset.
 *
 * @param[in] src       Unused
 * @param[in] ctx       Unused
 * @param[in] unused    Unused
 *
 **************************************************************************
 */

static void
VMToolsChannelReset(gpointer src,
                    ToolsAppCtx *ctx,
                    gpointer unused)
{
   GlobalConfigThreadState *threadState = &globalConfigThreadState;

   g_debug("%s: VMTools channel got reset.\n", __FUNCTION__);

   g_mutex_lock(&threadState->mutex);

   if (threadState->configInfo->pollInterval > 0) {
      /*
       * The RPC channel may get reset due to various conditions like
       * snapshotting the VM, vmotion the VM, instant cloning of the VM.
       * In order to avoid potential load spikes in case of instant clones,
       * wait for a randomized interval.
       */
      threadState->useRandomInterval = TRUE;
   }

   g_mutex_unlock(&threadState->mutex);
}


/*
 **************************************************************************
 * GlobalConfigToolsConfReload --                                    */ /**
 *
 * Callback function that gets called when the VMTools configuration gets
 * reloaded.
 *
 * @param[in] src       Unused
 * @param[in] ctx       Application Context
 * @param[in] unused    Unused
 *
 **************************************************************************
 */

static void
GlobalConfigToolsConfReload(gpointer src,
                            ToolsAppCtx *ctx,
                            gpointer unused)
{
   guint pollInterval;
   GlobalConfigThreadState *threadState = &globalConfigThreadState;
   gchar *newResourcePath;

   g_debug("%s: VMTools configuration got reloaded.\n", __FUNCTION__);

   g_mutex_lock(&threadState->mutex);

   newResourcePath = GLOBALCONF_GET_RESOURCE_PATH(ctx->config);
   if (g_strcmp0(newResourcePath,
                 threadState->configInfo->guestStoreResource)) {
      g_info("%s: '%s' changed. Old: %s, New: %s\n",
             __FUNCTION__, CONFNAME_GLOBALCONF_RESOURCE,
             threadState->configInfo->guestStoreResource,
             newResourcePath);
      g_free(threadState->configInfo->guestStoreResource);
      threadState->configInfo->guestStoreResource = newResourcePath;
   } else {
      g_free(newResourcePath);
      newResourcePath = NULL;
   }

   pollInterval = GlobalConfigGetPollInterval(ctx->config);
   if (pollInterval != threadState->configInfo->pollInterval) {
      g_info("%s: '%s' changed. Old: '%u', New: '%u' "
             "Signalling the change in the globalConfing configuration.\n",
             __FUNCTION__, CONFNAME_GLOBALCONF_POLL_INTERVAL,
             threadState->configInfo->pollInterval,
             pollInterval);
      threadState->configInfo->pollInterval = pollInterval;
      g_cond_signal(&threadState->cond);
   } else if (pollInterval == 0) {
      /*
       * Delete the stale config files (if any).
       */
      GlobalConfig_DeleteConfig();
   }

   g_mutex_unlock(&threadState->mutex);
}


/*
 **************************************************************************
 * GlobalConfGetConfPath --                                          */ /**
 *
 * Returns the path to the global configuration file.
 *
 * @return Path of the global configuration file. The caller shouldn't
 *         free the memory. Returns NULL if the path cannot be computed.
 *
 **************************************************************************
 */

static const gchar *
GlobalConfGetConfPath(void)
{
   static gchar *globalConfPath = NULL;

   if (globalConfPath == NULL) {
      char *guestAppConfPath = GuestApp_GetConfPath();
      if (guestAppConfPath == NULL) {
         g_warning("%s: Failed to get configuration directory.\n",
                   __FUNCTION__);
      } else {
         globalConfPath = g_build_filename(guestAppConfPath,
                                           GLOBALCONF_LOCAL_FILENAME,
                                           NULL);
      }
      free(guestAppConfPath);
   }

   return globalConfPath;
}


/*
 **************************************************************************
 * GlobalConfigThreadStateInit --                                    */ /**
 *
 * Reads the key/value pairs related to globalconf module from the user
 * specified configuration dictionary and intializes the globaconf module.
 *
 * @param[in] cfg    Tools configuration dictionary.
 *
 * @return TRUE if the globalconf module is initialized.
 *         FALSE if any error happened while initializing.
 *
 **************************************************************************
 */

static gboolean
GlobalConfigThreadStateInit(GKeyFile *cfg)
{
   gchar *guestAppConfPath;
   GlobalConfigInfo *configInfo;
   GlobalConfigThreadState *threadState = &globalConfigThreadState;

   ASSERT(cfg != NULL);

   guestAppConfPath = GuestApp_GetConfPath();
   if (guestAppConfPath == NULL) {
      g_warning("%s: Failed to get tools install path.\n", __FUNCTION__);
      return FALSE;
   }

   ASSERT(threadState->configInfo == NULL);

   configInfo = g_malloc0(sizeof(*configInfo));
   configInfo->localConfPath = g_build_filename(guestAppConfPath,
                                                GLOBALCONF_LOCAL_FILENAME,
                                                NULL);

   configInfo->localTempPath = g_build_filename(guestAppConfPath,
                                                GLOBALCONF_LOCAL_TEMP_FILENAME,
                                                NULL);

   vm_free(guestAppConfPath);

   configInfo->pollInterval = GlobalConfigGetPollInterval(cfg);

   g_debug("%s: %s: %d", __FUNCTION__, CONFNAME_GLOBALCONF_POLL_INTERVAL,
           configInfo->pollInterval);

   configInfo->guestStoreResource = GLOBALCONF_GET_RESOURCE_PATH(cfg);

   g_debug("%s: Configuration Resource path in GuestStore: %s",
           __FUNCTION__, configInfo->guestStoreResource);

   threadState->configInfo = configInfo;
   return TRUE;
}


/*
 **************************************************************************
 * GlobalConfigInfoFree --                                           */ /**
 *
 * Frees up memory allocated for GlobalConfigInfo structure.
 *
 * @param[in] data   GlobalConfigInfo structure that needs to be freed.
 *
 **************************************************************************
 */

static void
GlobalConfigInfoFree(GlobalConfigInfo *configInfo)
{
   if (configInfo != NULL) {
      g_free(configInfo->localConfPath);
      g_free(configInfo->localTempPath);
      g_free(configInfo->guestStoreResource);
   }

   g_free(configInfo);
}


/*
 **************************************************************************
 * GlobalConfigThreadStateFree --                                    */ /**
 *
 * Frees up memory allocated for GlobalConfigThread structure.
 *
 * @param[in] unused Callback data. Not used.
 *
 **************************************************************************
 */

static void
GlobalConfigThreadStateFree(gpointer unused)
{
   GlobalConfigThreadState *threadState = &globalConfigThreadState;

   GlobalConfigInfoFree(threadState->configInfo);
   threadState->configInfo = NULL;
}


/*
 **************************************************************************
 * GlobalConfigThreadTerminate --                                    */ /**
 *
 * Signals the 'global config' thread to exit.
 *
 * @param[in] ctx    Application context
 * @param[in] data   Pointer to GlobalConfigThreadState sturcture.
 *
 **************************************************************************
 */

static void
GlobalConfigThreadTerminate(ToolsAppCtx *ctx,
                            gpointer data)
{
   GlobalConfigThreadState *threadState = &globalConfigThreadState;

   g_mutex_lock(&threadState->mutex);

   threadState->terminate = TRUE;
   threadState->guestStoreEnabled = FALSE;

   g_cond_signal(&threadState->cond);

   g_mutex_unlock(&threadState->mutex);
}


/*
 **************************************************************************
 * LoadConfigFile --                                                 */ /**
 *
 * Loads the specified config file.
 *
 * @param[in] confPath Path to the configuration file.
 * @param[in|out] config  Configuration dictionary that is loaded with the
 *                        contents from the specified configuration file. When
 *                        loading, the old content is destroyed. Before
 *                        invoking this function the first time for a specific
 *                        confPath, the config must be initialized to NULL.
 * @param[in|out] mtime   Last known modification time of the config file.
 *                        When the function succeeds, will contain the new
 *                        modification time read from the file. If NULL (or 0),
 *                        the configuration dictionary is always loaded.
 *
 * @return TRUE Whether a new configuration dictionary was loaded.
 *
 **************************************************************************
 */

static gboolean
LoadConfigFile(const gchar *confPath,
               GKeyFile **config,
               time_t *mtime)
{
   gboolean configUpdated = FALSE;
   GStatBuf confStat;
   GError *err = NULL;
   GKeyFile *cfg;

   if (config == NULL || confPath == NULL ) {
      g_debug("%s: Invalid arguments specified.\n", __FUNCTION__);
      goto exit;
   }

  if (g_stat(confPath, &confStat) == -1) {
      /*
       * If the file doesn't exist, it's not an error.
       */
      if (errno != ENOENT) {
         g_warning("%s: Failed to stat conf file: %s, Error: '%s'\n",
                   __FUNCTION__, confPath, strerror(errno));
      } else {
         /*
          * If we used to have a file, set the config to NULL.
          */
         if (*config != NULL) {
            g_key_file_free(*config);
            *config = NULL;
            configUpdated = TRUE;
         }
      }
      goto exit;
   }

   /* Check if we really need to load the data. */
   if (mtime != NULL && confStat.st_mtime <= *mtime) {
      goto exit;
   }

   cfg = g_key_file_new();

   /* On error, 'err' will be set, null otherwise. */
   /* coverity[check_return] */
   g_key_file_load_from_file(cfg, confPath,
                             G_KEY_FILE_NONE, &err);
   if (err != NULL) {
      g_warning("%s: Failed to load the configuration from '%s'. Error: '%s'",
                __FUNCTION__, confPath, err->message);
      g_clear_error(&err);
      g_key_file_free(cfg);
      cfg = NULL;
      goto exit;
   }

   if (*config != NULL) {
      g_key_file_free(*config);
   }
   *config = cfg;
   configUpdated = TRUE;

   if (mtime != NULL) {
      *mtime = confStat.st_mtime;
   }

   g_debug("%s: Loaded the configuration from %s.\n", __FUNCTION__, confPath);

exit:
   return configUpdated;
}


/*
 **************************************************************************
 * DownloadConfig --                                                 */ /**
 *
 * Downloads the tools.conf from the GuestStore.
 *
 * @param[in] guestStoreResource Resource path in the GuestStore.
 * @param[in|opt] localTempPath  File path to be used for temporarily download
 *                               of the resource from the GuestStore. If this is
 *                               NULL, then a random file path is used.
 *
 * @return GuestStoreClientError.
 **************************************************************************
 */

static GuestStoreClientError
DownloadConfig(const char *guestStoreResource,
               const char *localTempPath)
{
   GuestStoreClientError status = GSLIBERR_GENERIC;
   const gchar *localConfPath = GlobalConfGetConfPath();
   gchar *randomLocalTempPath = NULL;

   if (localConfPath == NULL) {
      g_warning("%s: Failed to get the configuration file path.\n",
                __FUNCTION__);
      return status;
   }

   if (localTempPath == NULL) {
      int fd = File_MakeSafeTemp(NULL, &randomLocalTempPath);
      if (fd != -1) {
         close(fd);
      } else {
         g_warning("%s: Failed to get the random temporary file.\n",
                   __FUNCTION__);
         return status;
      }
      localTempPath = randomLocalTempPath;
   }

   g_debug("%s: Downloading the configuration to %s\n", __FUNCTION__,
           localTempPath);

   status = GuestStoreClient_GetContent(guestStoreResource,
                                        localTempPath,
                                        NULL, NULL);
   if (status == GSLIBERR_SUCCESS) {
      GKeyFile *newGlobalCfg = NULL;

      g_debug("%s: Successfully downloaded the configuration from GuestStore.",
              __FUNCTION__);

      LoadConfigFile(localTempPath, &newGlobalCfg, NULL);

      if (newGlobalCfg != NULL) {
         GKeyFile *existingGlobalCfg = NULL;

         LoadConfigFile(localConfPath, &existingGlobalCfg, NULL);
         if (!VMTools_CompareConfig(existingGlobalCfg, newGlobalCfg)) {
            /*
             * Write the config to the filesystem using VMTools_WriteConfig and
             * the content will be normalized.
             */
            VMTools_WriteConfig(localConfPath,
                                newGlobalCfg,
                                NULL);
         }

         if (existingGlobalCfg != NULL) {
            g_key_file_free(existingGlobalCfg);
         }

         g_key_file_free(newGlobalCfg);
      }
   } else {
      g_debug("%s: Failed to download the configuration "
              "from GuestStore. Error: %d",
              __FUNCTION__, status);
      /*
       * If the global configuration is not available in the GuestStore or
       * VM is not allowed to access it, then delete the local copy of global
       * configuration downloaded previously.
       */
      if (status == GSLIBERR_CONTENT_NOT_FOUND ||
          status == GSLIBERR_CONTENT_FORBIDDEN) {
         File_UnlinkIfExists(localConfPath);
      }
   }

   File_UnlinkIfExists(localTempPath);
   g_free(randomLocalTempPath);
   return status;
}


/*
 **************************************************************************
 * GlobalConfigThreadStart --                                        */ /**
 *
 * Entry function for the thread to download the global configuration from
 * the GuestStore.
 *
 * @param[in] ctx    Application context
 * @param[in] unused Callback data. Not used.
 *
 **************************************************************************
 */

static void
GlobalConfigThreadStart(ToolsAppCtx *ctx,
                        gpointer unused)
{
   GlobalConfigThreadState *threadState = &globalConfigThreadState;
   gboolean waitBeforeDownload = FALSE;

   g_mutex_lock(&threadState->mutex);

   while (!threadState->terminate) {
      GlobalConfigInfo *configInfo = threadState->configInfo;
      if (threadState->guestStoreEnabled && configInfo->pollInterval > 0) {
         gint64 endTime;

         if (waitBeforeDownload) {
            waitBeforeDownload = FALSE;
            endTime = g_get_monotonic_time() +
                     (configInfo->pollInterval * G_TIME_SPAN_SECOND);
            g_cond_wait_until(&threadState->cond, &threadState->mutex, endTime);
            continue;
         }  else if (threadState->useRandomInterval) {
            threadState->useRandomInterval = FALSE;
            endTime = g_get_monotonic_time() +
                      (GenerateRandomInterval() * G_TIME_SPAN_SECOND);
            g_cond_wait_until(&threadState->cond,
                              &threadState->mutex, endTime);
            continue;
         }

         g_mutex_unlock(&threadState->mutex);

         DownloadConfig(configInfo->guestStoreResource,
                        configInfo->localTempPath);

         g_mutex_lock(&threadState->mutex);

         waitBeforeDownload = TRUE;
      } else {
         if (configInfo->pollInterval == 0) {
            GlobalConfig_DeleteConfig();
         }
         g_cond_wait(&threadState->cond, &threadState->mutex);
         waitBeforeDownload = FALSE;
      }
   }

   g_mutex_unlock(&threadState->mutex);
}


/*
 **************************************************************************
 * GlobalConfig_Start --                                             */ /**
 *
 * Initializes the global config module. If the feature is not enabled in
 * tools.conf file, the module is not enabled. If this function is called
 * in the context of the Tools main service, a thread is started in the
 * background to periodically download the global configuration from the
 * GuestStore.
 *
 * @param[in] ctx                   Application context.
 *
 * @return TRUE if the download config module is successfully started
 *
 **************************************************************************
 */

gboolean
GlobalConfig_Start(ToolsAppCtx *ctx)
{
   gboolean ret = FALSE;
   GKeyFile *cfg;

   ASSERT(ctx != NULL);
   cfg = ctx->config;
   ASSERT(cfg != NULL);

   if (!GlobalConfigThreadStateInit(cfg)) {
      g_warning("%s: Failed to initialize global config module.",
                  __FUNCTION__);
      goto exit;
   }

   if (TOOLS_IS_MAIN_SERVICE(ctx)) {
      /*
       * Start the background thread only when this module is started by
       * 'vmsvc' service.
       */
      ret = ToolsCorePool_StartThread(ctx,
                                      "toolsGlobalConfig",
                                      GlobalConfigThreadStart,
                                      GlobalConfigThreadTerminate,
                                      NULL,
                                      GlobalConfigThreadStateFree);
      if (!ret) {
         g_info("%s: Unable to start the GuestStore download config thread",
                __FUNCTION__);
         GlobalConfigThreadStateFree(NULL);
         goto exit;
      }

      if (g_signal_lookup(TOOLS_CORE_SIG_GUESTSTORE_STATE,
                          G_OBJECT_TYPE(ctx->serviceObj)) != 0) {
         g_signal_connect(ctx->serviceObj,
                          TOOLS_CORE_SIG_GUESTSTORE_STATE,
                          G_CALLBACK(GuestStoreStateChanged),
                          NULL);
      }

      if (g_signal_lookup(TOOLS_CORE_SIG_RESET,
                          G_OBJECT_TYPE(ctx->serviceObj)) != 0) {
         g_signal_connect(ctx->serviceObj,
                          TOOLS_CORE_SIG_RESET,
                          G_CALLBACK(VMToolsChannelReset),
                          NULL);
      }
   } else {
      ret = TRUE;
   }

   if (g_signal_lookup(TOOLS_CORE_SIG_CONF_RELOAD,
                       G_OBJECT_TYPE(ctx->serviceObj)) != 0) {
      g_signal_connect(ctx->serviceObj,
                       TOOLS_CORE_SIG_CONF_RELOAD,
                       G_CALLBACK(GlobalConfigToolsConfReload),
                       NULL);
   }

exit:
   return ret;
}


/*
 **************************************************************************
 * GlobalConfig_LoadConfig --                                        */ /**
 *
 * Loads the Global configuration downloaded from the GuestStore. The
 * modification time of the configuration file is checked and it's loaded only
 * if it has been updated since the caller specified modification time.
 *
 * @param[in|out] config  Configuration dictionary that is loaded with the
 *                        contents downloaded from the GuestStore. When
 *                        loading, the old content is destroyed. The caller
 *                        must initialize this to NULL before the first
 *                        invocation of this function.
 * @param[in|out] mtime   Last known modification time of the config file.
 *                        When the function succeeds, will contain the new
 *                        modification time read from the file. If NULL (or 0),
 *                        the configuration dictionary is always loaded.
 *
 * @return TRUE Whether a new configuration dictionary was loaded.
 *
 **************************************************************************
 */

gboolean
GlobalConfig_LoadConfig(GKeyFile **config,
                        time_t *mtime)
{
   static const gchar *confPath = NULL;

   if (confPath == NULL) {
      confPath = GlobalConfGetConfPath();
   }

   if (confPath != NULL) {
      return LoadConfigFile(confPath, config, mtime);
   } else {
      return FALSE;
   }
}


/*
 **************************************************************************
 * GlobalConfig_GetEnabled --                                        */ /**
 *
 * Query the given configuration dictionary and returns the status of
 * globaconf module.
 *
 * @param[in] config  Configuration dictionary that needs to be queried.
 *
 * @return TRUE if the globalconf module is enabled. FALSE otherwise.
 *
 **************************************************************************
 */

gboolean
GlobalConfig_GetEnabled(GKeyFile *config)
{
   return VMTools_ConfigGetBoolean(config,
                                   CONFGROUPNAME_GLOBALCONF,
                                   CONFNAME_GLOBALCONF_ENABLED,
                                   GLOBALCONF_DEFAULT_ENABLED);
}


/*
 **************************************************************************
 * GlobalConfig_SetEnabled --                                        */ /**
 *
 * Changes the 'enabled' status of globalconf module in the specified
 * configuration dictionary.
 *
 * @param[in]     enabled Desired state of the globalconf module.
 * @param[in|out] config  Configuration dictionary that needs to be updated.
 *
 **************************************************************************
 */

void
GlobalConfig_SetEnabled(gboolean enabled,
                        GKeyFile *config)
{
   if (config != NULL) {
      g_key_file_set_boolean(config, CONFGROUPNAME_GLOBALCONF,
                             CONFNAME_GLOBALCONF_ENABLED, enabled);
   }
}


/*
 **************************************************************************
 * GlobalConfig_DeleteConfig --                                      */ /**
 *
 * Delete the global configuration downloaded from the GuestStore.
 *
 * @return TRUE if the global configuration is successfully deleted.
 *         FALSE if any error happens.
 *
 **************************************************************************
 */

gboolean
GlobalConfig_DeleteConfig(void) {
   const gchar *confPath = GlobalConfGetConfPath();

   return confPath != NULL && File_UnlinkIfExists(confPath) == 0;
}


/*
 **************************************************************************
 * GlobalConfig_DownloadConfig --                                    */ /**
 *
 * Download the global configuration from the GuestStore.
 *
 * @param[in] config  Configuration dictionary.
 *
 * @return GuestStoreClientError
 *
 **************************************************************************
 */

GuestStoreClientError
GlobalConfig_DownloadConfig(GKeyFile *config)
{
   GuestStoreClientError status = GSLIBERR_GENERIC;
   char *guestStoreResource;

   if (config == NULL) {
      g_warning("%s: Invalid arguments specified.\n", __FUNCTION__);
      return status;
   }

   guestStoreResource = GLOBALCONF_GET_RESOURCE_PATH(config);

   status = DownloadConfig(guestStoreResource, NULL);

   g_free(guestStoreResource);

   return status;
}
