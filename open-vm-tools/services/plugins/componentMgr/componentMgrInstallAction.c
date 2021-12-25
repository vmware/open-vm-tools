/*********************************************************
 * Copyright (C) 2021 VMware, Inc. All rights reserved.
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
 * componentMgrInstallAction.c --
 *
 * Functions to manage the known and enabled components by the componentMgr.
 * Consists of functions periodically handling adding/removing of
 * components in the guest OS. Periodically read the guestVar
 * guestinfo./vmware.components.<comp_name>.desiredstate and take present or
 * absent action on the components.
 * Adding/removing a component managed by the plugin is performed
 * asynchronously using Proc_Manager API's.
 *
 */


#include "componentMgrPlugin.h"
#include "str.h"
#include "file.h"
#include "util.h"
#include "guestApp.h"
#include "codeset.h"


/*
 * Structure to store information about the scripts to be invoked for
 * present/absent action on the components by the plugin.
 * The script to be invoked shall be predefined with default arguments
 * in the structure according to actions present/absent/checkstatus.
 */

typedef struct ComponentAction {
   const char *componentName;              /* The name of the enabled
                                              component. */

   const char *scriptName;                 /* The default script to be invoked
                                              to take actions for a particular
                                              component */

   const char *addActionArguments;         /* Default arguments to the script
                                              to execute present action towards
                                              the component in the guest OS. */

   const char *removeActionArguments;      /* Default arguments to the script
                                              to execute absent action towards
                                              the component in the guest OS. */

   const char *checkStatusActionArguments; /* Default arguments to the script
                                              to execute checkstatus towards
                                              the component in the guest OS. */

   const char* mandatoryParameters;        /* Arguments that are mandatory to
                                              be passed to script. */

   const char *componentDirectory;         /* The name of directory in which
                                              scripts will be installed.*/

   char* (*customizeRemoveAction)();       /* A custom callback function
                                              to customize arguments for
                                              absent action on the component
                                              script. */

   char* (*customizeAddAction)();          /* A custom callback function
                                              to customize arguments for
                                              present action on the component
                                              script. */
} ComponentAction;


static char*
ComponentMgrCustomizeSaltAddAction();


/*
 * An array to store all the state information of the component over the
 * life time for the plugin. The array consists of cached values of enabled
 * status, async process info, GSource timers, status count down counter and
 * current action to be run on the component.
 */

static struct ComponentInfo components[] = {
   {SALT_MINION, TRUE, NOTINSTALLED, NULL, NULL, COMPONENTMGR_CHECK_STATUS_COUNT_DOWN, INVALIDACTION}
};

/*
 * An array containing information of the component and its related
 * scripts and respective default arguments for actions present/absent
 * or checkstatus.
 */
#if defined(_WIN32)
static const char powershellExecutable[] = "\\WindowsPowerShell\\v1.0\\PowerShell.exe";

static const char componentMgrExecutionPolicy[] = "-ExecutionPolicy RemoteSigned -File";

static ComponentAction executionScripts[] = {
   {SALT_MINION,"svtminion.ps1", "-Install", "-Remove", "-Status", "-Loglevel debug", "saltMinion", NULL, &ComponentMgrCustomizeSaltAddAction}
};
#else
static ComponentAction executionScripts[] = {
   {SALT_MINION,"svtminion.sh", "--install", "--remove", "--status", "--loglevel debug", "saltMinion", NULL, &ComponentMgrCustomizeSaltAddAction}
};
#endif


/*
 *****************************************************************************
 * ComponentMgr_GetComponentName --
 *
 * This function fetches the name of component from the components array.
 *
 * @param[in] componentIndex Index of the component in the global array of
 *                           components.
 *
 * @return
 *      Name of component.
 *
 * Side effects:
 *      None.
 *
 *****************************************************************************
 */

const char*
ComponentMgr_GetComponentName(int componentIndex) // IN
{
   return components[componentIndex].name;
}


/*
 *****************************************************************************
 * ComponentMgr_CheckAnyAsyncProcessRunning --
 *
 * This function checks if any async process is running for any component
 * inside the plugin.
 *
 * @return
 *      FALSE if no async process is running for a component.
 *      TRUE if an async process is running for a component.
 *
 * Side effects:
 *      None.
 *
 *****************************************************************************
 */

gboolean
ComponentMgr_CheckAnyAsyncProcessRunning() // IN
{
   int i;
   for (i = 0; i < ARRAYSIZE(components); i++) {
      if (ComponentMgr_IsAsyncProcessRunning(i)) {
         return TRUE;
      }
   }
   return FALSE;
}


/*
 *****************************************************************************
 * ComponentMgr_IsAsyncProcessRunning --
 *
 * This function indicates whether any async process is already running for
 * a component.
 *
 * @param[in] componentIndex Index of the component in the global array of
 *                           components.
 *
 * @return
 *      FALSE if no async process is running for a component.
 *      TRUE if an async process is running for a component.
 *
 * Side effects:
 *      None.
 *
 *****************************************************************************
 */

gboolean
ComponentMgr_IsAsyncProcessRunning(int componentIndex) // IN
{
   if (components[componentIndex].procInfo != NULL) {
      g_info("%s: Component %s has an async process still running.\n",
             __FUNCTION__, components[componentIndex].name);
      return TRUE;
   } else {
      return FALSE;
   }
}


/*
 *****************************************************************************
 * ComponentMgr_SetComponentAsyncProcInfo --
 *
 * This function caches info of the async process currently running for a
 * component.
 *
 * @param[in] asyncProcInfo  An asyncProcInfo object of the currently running
                             async process.
 * @param[in] componentIndex Index of the component in the global array of
 *                           components.
 *
 * @return
 *      None.
 *
 * Side effects:
 *      None.
 *
 *****************************************************************************
 */

void
ComponentMgr_SetComponentAsyncProcInfo(AsyncProcessInfo *asyncProcInfo, // IN
                                       int componentIndex)              // IN
{
   ASSERT(components[componentIndex].procInfo == NULL);
   components[componentIndex].procInfo = asyncProcInfo;
}


/*
 *****************************************************************************
 * ComponentMgr_ResetComponentAsyncProcInfo --
 *
 * This function resets the state of any async process running for a component.
 *
 * @param[in] componentIndex Index of the component in the global array of
 *                           components.
 *
 * @return
 *      None.
 *
 * Side effects:
 *      None.
 *
 *****************************************************************************
 */

void
ComponentMgr_ResetComponentAsyncProcInfo(int componentIndex) // IN
{
   components[componentIndex].procInfo = NULL;
}


/*
 *****************************************************************************
 * ComponentMgr_SetComponentGSourceTimer --
 *
 * This function caches the GSource timer running for an async process for a
 * component. The timer is used to monitor the state of async process.
 *
 * @param[in] componentTimer A GSource timer created when performing present
 *                           or absent action on a component.
 * @param[in] componentIndex Index of the component in the global array of
 *                           components.
 *
 * @return
 *      None.
 *
 * Side effects:
 *      Sets the GSource timer of an async process running for a component.
 *
 *****************************************************************************
 */

void
ComponentMgr_SetComponentGSourceTimer(GSource *componentTimer, // IN
                                      int componentIndex)      // IN
{
   ASSERT(components[componentIndex].sourceTimer == NULL);
   components[componentIndex].sourceTimer = componentTimer;
}


/*
 *****************************************************************************
 * ComponentMgr_ResetComponentGSourceTimer --
 *
 * This function resets the component GSource timer to make way for creation
 * a new async process.
 *
 * @param[in] componentIndex Index of the component in the global array of
 *                           components.
 * @return
 *      None.
 *
 * Side effects:
 *      Reset the GSource timer of an async process running for a component.
 *
 *****************************************************************************
 */

void
ComponentMgr_ResetComponentGSourceTimer(int componentIndex) // IN
{
   components[componentIndex].sourceTimer = NULL;
}


/*
 *****************************************************************************
 * ComponentMgrGetScriptFullPath --
 *
 * This function returns a full path to the component script based on the
 * installed path of open-vm-tools or VMware Tools.
 *
 * @param[in] scriptName Name of the component script.
 * @param[in] componentDir Directory of the component.
 *
 * @return
 *      A full path to the script file based on open-vm-tools or VMware Tools
 *      installation.
 *
 * Side effects:
 *      Caller needs to free the full script path value.
 *
 *****************************************************************************
 */

static gchar*
ComponentMgrGetScriptFullPath(const char *scriptName,   // IN
                              const char *componentDir) // IN
{
   gchar *scriptInstallDir;
   gchar *toolsInstallDir;

#if defined(OPEN_VM_TOOLS)
   toolsInstallDir = Util_SafeStrdup(VMTOOLS_COMPONENTMGR_PATH);
   scriptInstallDir = g_strdup_printf("%s%s%s%s", toolsInstallDir,
                                      componentDir, DIRSEPS, scriptName);
#else
   toolsInstallDir = GuestApp_GetInstallPath();
   scriptInstallDir = g_strdup_printf("%s%s%s%s%s%s%s", toolsInstallDir, DIRSEPS,
                                      COMPONENTMGR_DIRECTORY, DIRSEPS,
                                      componentDir, DIRSEPS, scriptName);
#endif

   g_free(toolsInstallDir);
   return scriptInstallDir;
}


/*
 *****************************************************************************
 * ComponentMgrCustomizeSaltAddAction --
 *
 * This function customizes the custom arguments for the present action for the
 * salt component script.
 *
 * @return
 *      A customized argument string for the salt component script.
 *
 * Side effects:
 *      Caller has to free the returned custom arguments.
 *
 *****************************************************************************
 */

static char*
ComponentMgrCustomizeSaltAddAction()
{
   size_t replylen;
   gchar *msg;
   gboolean status;
   char *actionArguments = NULL;

   msg = g_strdup_printf("%s.%s.args", COMPONENTMGR_ACTION, SALT_MINION);
   status = ComponentMgr_SendRpc(ComponentMgr_GetToolsAppCtx(), msg,
                                 &actionArguments, &replylen);
   g_free(msg);

   if (!status) {
      vm_free(actionArguments);
      return NULL;
   }

   return actionArguments;
}


/*
 *****************************************************************************
 * ComponentMgrConstructCommandline --
 *
 * The function constructs the commandline for linux and windows to execute
 * the script as an async process to perform present/absent action on a
 * component.
 *
 * The windows counterpart is constructed as:
 * <path to powershell.exe> -ExecutionPolicy RemoteSigned -File \
 * <path to component script> <args to component script>
 *
 * The linux counterpart is constructed as:
 * <path to component script> <argumnets to the script>
 *
 * @param[in] scriptName Name of the component script.
 * @param[in] defaultArguments Default arguments to the component script.
 * @param[in] mandatoryParams mandatory params to the component script.
 * @param[in] customizeAction A callback function to customize the arguments
                              for the component script.
 *
 * @return
 *      A commandline to be directly run as an async process.
 *
 * Side effects:
 *      Caller needs to free the commandline.
 *
 *****************************************************************************
 */

static char *
ComponentMgrConstructCommandline(gchar *scriptPath,            // IN
                                 const char *defaultArguments, // IN
                                 const char *mandatoryParams,  // IN
                                 char* (*customizeAction)())   // IN
{
   char *commandline = NULL;
   const char *mandatoryParamsExists = NULL;
   char *customArguments = NULL;
#if defined(_WIN32)
   WCHAR sysDirW[MAX_PATH];
   UINT retLen;
   char *sysDir = NULL;
#endif

   // Customize the arguments for the specific action via the callback function
   if (customizeAction != NULL) {
      g_info("%s: Customizing arguments with function.\n", __FUNCTION__);
      customArguments = customizeAction();
   }

#if defined(_WIN32)
   retLen = GetSystemDirectoryW(sysDirW, _countof(sysDirW));
   if (retLen == 0 || retLen >= _countof(sysDirW)) {
      g_warning("%s: Unable to get system directory.\n", __FUNCTION__);
      goto proceedexit;
   }

   if (!CodeSet_Utf16leToUtf8((const char *)sysDirW,
                              wcslen(sysDirW) * sizeof sysDirW[0],
                              &sysDir,
                              NULL)) {
      g_warning("%s: Could not convert system directory to UTF-8.\n",
                __FUNCTION__);
      goto proceedexit;
   }

   if (customArguments != NULL) {
      mandatoryParamsExists = strstr(customArguments, mandatoryParams);
      if (mandatoryParamsExists == NULL) {
         commandline = Str_SafeAsprintf(NULL, "\"%s%s\" %s \"%s\" %s %s %s",
                                        sysDir, powershellExecutable,
                                        componentMgrExecutionPolicy,
                                        scriptPath,
                                        defaultArguments, customArguments,
                                        mandatoryParams);
      } else {
         commandline = Str_SafeAsprintf(NULL, "\"%s%s\" %s \"%s\" %s %s",
                                        sysDir, powershellExecutable,
                                        componentMgrExecutionPolicy,
                                        scriptPath,
                                        defaultArguments, customArguments);

      }
   } else {
      commandline = Str_SafeAsprintf(NULL, "\"%s%s\" %s \"%s\" %s %s",
                                     sysDir, powershellExecutable,
                                     componentMgrExecutionPolicy,
                                     scriptPath,
                                     defaultArguments,
                                     mandatoryParams);
   }

   free(sysDir);
#else
   if (customArguments != NULL) {
      mandatoryParamsExists = strstr(customArguments, mandatoryParams);
      if (mandatoryParamsExists == NULL) {
         commandline = Str_SafeAsprintf(NULL, "%s %s %s %s", scriptPath,
                                        defaultArguments, customArguments,
                                        mandatoryParams);
      } else {
         commandline = Str_SafeAsprintf(NULL, "%s %s %s", scriptPath,
                                        defaultArguments, customArguments);

      }
   } else {
      commandline = Str_SafeAsprintf(NULL, "%s %s %s", scriptPath,
                                     defaultArguments, mandatoryParams);
   }
   // To satisfy a complier warning of missing label.
   goto proceedexit;
#endif

proceedexit:
   if (customArguments != NULL) {
      vm_free(customArguments);
   }
   return commandline;
}


/*
 *****************************************************************************
 * ComponentMgr_CheckStatusCommandLine --
 *
 * This function fetches the commandline needed to be executed to check the
 * current status of component installation. It provides the commandline as
 * <component_script> <checkstatus_arguments>
 *
 * @param[in] componentIndex Index of the component in the global array of
 *                           components.
 *
 * @return
 *      Commandline to check current status of component.
 *
 * Side effects:
 *      Caller needs to free the commandline value.
 *
 *****************************************************************************
 */

char *
ComponentMgr_CheckStatusCommandLine(int componentIndex) // IN
{
   char *commandline;
   gchar *scriptFullPath;

   /*
    * Always check for component enabled state before proceeding, since check
    * status can be called at any part of component action.
    */
   if (!components[componentIndex].isEnabled) {
      g_info("%s: Component %s is disabled.\n", __FUNCTION__,
             components[componentIndex].name);
      return NULL;
   }

   scriptFullPath = ComponentMgrGetScriptFullPath(executionScripts[componentIndex].scriptName,
                                                  executionScripts[componentIndex].componentDirectory);
   if (!File_Exists(scriptFullPath)) {
      g_info("%s: Script file for component %s does not exist at path %s.\n",
             __FUNCTION__, components[componentIndex].name, scriptFullPath);
      return NULL;
   }

   commandline = ComponentMgrConstructCommandline(scriptFullPath,
                                                  executionScripts[componentIndex].checkStatusActionArguments,
                                                  executionScripts[componentIndex].mandatoryParameters,
                                                  NULL);

   g_free(scriptFullPath);

   return commandline;
}


/*
 *****************************************************************************
 * ComponentMgrSetEnabledComponentInfo --
 *
 * This function sets the isEnabled state of the component which determines
 * whether the component managed by the plugin is enabled or disabled.
 *
 * @param[in] componentName Name of the component to enable or disable.
 * @param[in] enabled A boolean value indicating component enabled status.
 *
 * @return
 *      None
 * Side effects:
 *      Sets the enabled/disabled status of a component.
 *
 *****************************************************************************
 */

static void
ComponentMgrSetEnabledComponentInfo(const char *componentName, // IN
                                    gboolean enabled)          // IN
{
   int i;
   gboolean componentFound = FALSE;

   for (i = 0; i < ARRAYSIZE(components); i++) {
      if (Str_Strcmp(components[i].name, componentName) == 0) {
         components[i].isEnabled = enabled;
         componentFound = TRUE;
         break;
      }
   }

   if (!componentFound) {
      g_info("%s: Invalid component name %s.\n",
             __FUNCTION__, componentName);
   }
}


/*
 *****************************************************************************
 * ComponentMgr_SetStatusComponentInfo --
 *
 * This function sets the status of the component managed by the plugin and
 * publishes guestVar guestinfo.vmware.components.<comp_name>.laststatus.
 *
 * @param[in] ctx Tools application context.
 * @param[in] componentIndex Index of the component in the global array of
 *                           components.
 * @param[in] exitCode The exit code of the async process running check status
 *                     operation on a component.
 *
 * @return
 *      None.
 *
 * Side effects:
 *      Sets the current status for a component.
 *
 *****************************************************************************
 */

void
ComponentMgr_SetStatusComponentInfo(ToolsAppCtx *ctx,   // IN
                                    int exitCode,       // IN
                                    int componentIndex) // IN
{
   gchar *msg;
   gboolean status;

   msg = g_strdup_printf("%s.%s.%s %d", COMPONENTMGR_PUBLISH_COMPONENTS,
                         components[componentIndex].name,
                         COMPONENTMGR_INFOLASTSTATUS,
                         exitCode);

   status = ComponentMgr_SendRpc(ctx, msg, NULL, NULL);
   g_free(msg);
   components[componentIndex].status = exitCode;
}


/*
 *****************************************************************************
 * ComponentMgrSetEnabledAllComponents --
 *
 * This function enables/disables all the components managed by the plugin.
 *
 * @param[in] enabled A boolean value to enable/disable all components.
 *
 * @return
 *      None.
 *
 * Side effects:
 *      None.
 *
 *****************************************************************************
 */

static void
ComponentMgrSetEnabledAllComponents(gboolean enabled) // IN
{
   int i;
   for (i = 0; i < ARRAYSIZE(components); i++) {
      components[i].isEnabled = enabled;
   }
}


/*
 *****************************************************************************
 * ComponentMgr_ExecuteComponentAction --
 *
 * This function validates the current status of the component against
 * current action be to be taken for a component and constructs a commandline
 * to execute present/absent action on a component as an async process.
 *
 * @param[in] componentIndex Index of the component in the global array of
 *                           components.
 *
 * @return
 *      None.
 *
 * Side effects:
 *      None.
 *
 *****************************************************************************
 */

void
ComponentMgr_ExecuteComponentAction(int componentIndex) // IN
{
   gchar *scriptFullPath;
   const char *action = NULL;
   const char *defaultArguments = NULL;
   char *commandline = NULL;
   char* (*customizeAction)() = NULL;
   Action installaction = components[componentIndex].action;

   if (!components[componentIndex].isEnabled) {
      g_debug("%s: Component %s is disabled", __FUNCTION__,
              components[componentIndex].name);
      return;
   }

   action = ComponentMgr_GetComponentAction(installaction);
   if((Str_Strcmp(action, COMPONENTMGR_COMPONENTPRESENT) == 0) &&
      (components[componentIndex].status == NOTINSTALLED ||
      components[componentIndex].status == INSTALLFAILED ||
      components[componentIndex].status == REMOVEFAILED)) {
      installaction = PRESENT;
   } else if((Str_Strcmp(action, COMPONENTMGR_COMPONENTABSENT) == 0) &&
             (components[componentIndex].status == INSTALLED ||
             components[componentIndex].status == INSTALLFAILED ||
             components[componentIndex].status == REMOVEFAILED)) {
      installaction = ABSENT;
   } else {
      g_debug("%s: Action %s will not be executed for component %s with "
              "current status %s.\n", __FUNCTION__, action,
              components[componentIndex].name,
              ComponentMgr_GetComponentInstallStatus(components[componentIndex].status));
      return;
   }

   g_info("%s: Executing action %s for component %s current status %s.\n",
          __FUNCTION__, action, components[componentIndex].name,
          ComponentMgr_GetComponentInstallStatus(components[componentIndex].status));

   /*
    * The main logic which handles the present/absent action on a component.
    * Internally spins off async process to add/remove the component
    * on the guest.
    */
   switch(installaction) {
      case PRESENT:
         defaultArguments = executionScripts[componentIndex].addActionArguments;
         customizeAction = executionScripts[componentIndex].customizeAddAction;
         break;
      case ABSENT:
         defaultArguments = executionScripts[componentIndex].removeActionArguments;
         customizeAction = executionScripts[componentIndex].customizeRemoveAction;
         break;
      default:
         break;
   } // end switch
   scriptFullPath = ComponentMgrGetScriptFullPath(executionScripts[componentIndex].scriptName,
                                                  executionScripts[componentIndex].componentDirectory);

   commandline = ComponentMgrConstructCommandline(scriptFullPath,
                                                  defaultArguments,
                                                  executionScripts[componentIndex].mandatoryParameters,
                                                  customizeAction);
    g_free(scriptFullPath);

   if (commandline == NULL) {
      g_info("%s: Construction of command line failed for component %s.\n",
             __FUNCTION__, components[componentIndex].name);
      return;
   }

   g_info("%s: Commandline %s to perform %s action on component %s.\n",
          __FUNCTION__, commandline, action, components[componentIndex].name);
   ComponentMgr_AsynchronousComponentActionStart(ComponentMgr_GetToolsAppCtx(),
                                                 commandline, componentIndex);
   free(commandline);
}


/*
 *****************************************************************************
 * ComponentMgrPublishKnownComponents --
 *
 * This function publishes guestVar guestinfo.vmware.components.available
 * with all the components managed by the plugin.
 *
 * @param[in] ctx Tools application context
 *
 * @return
 *      None.
 *
 * Side effects:
 *      Updates the enabled/disabled status of the all the components managed
 *      by the plugin.
 *
 *****************************************************************************
 */

static void
ComponentMgrPublishKnownComponents(ToolsAppCtx *ctx) // IN
{
   int i;
   DynBuf enabledComponents;
   DynBuf_Init(&enabledComponents);

   for (i = 0; i < ARRAYSIZE(components); i++) {
      if (components[i].isEnabled) {
         gchar *scriptFullPath;
         /*
          * We need to check the existence of the script for a particular
          * component before we begin the preset/absent action on the component.
          * Skipping the component if no script is installed.
          */
         scriptFullPath = ComponentMgrGetScriptFullPath(executionScripts[i].scriptName,
                                                        executionScripts[i].componentDirectory);

         if (!File_Exists(scriptFullPath)) {
            g_info("%s: Script file for component %s does not exist "
                   "under path %s.\n", __FUNCTION__, components[i].name,
                   scriptFullPath);
            g_free(scriptFullPath);
            components[i].isEnabled = FALSE;
            continue;
         }

         g_free(scriptFullPath);

         if (DynBuf_GetSize(&enabledComponents) != 0) {
            DynBuf_Append(&enabledComponents, ",", 1);
         }
         DynBuf_Append(&enabledComponents, components[i].name,
                       strlen(components[i].name));
      }
   }

   if (DynBuf_GetSize(&enabledComponents) == 0) {
      ComponentMgr_PublishAvailableComponents(ctx, COMPONENTMGR_NONECOMPONENTS);
   } else {
      ComponentMgr_PublishAvailableComponents(ctx,
                                              DynBuf_GetString(&enabledComponents));
   }

   DynBuf_Destroy(&enabledComponents);
}


/*
 *****************************************************************************
 * ComponentMgrIncludedComponents --
 *
 * This function checks and validates the comma seperated list fetched from
 * included tools.conf configuration and classifies the first occurrence of
 * all or none which are special values and returns the result.
 *
 * @param[in] componentString Comma seperated string from the included
 *                            tools.conf configuration.
 *
 * @retun
 *      Classify first occurrence of all or none in the string and return.
 *
 * Side effects
 *      None.
 *
 *****************************************************************************
 */

static IncludedComponents
ComponentMgrIncludedComponents(const char* componentString) // IN
{
   int i;
   gchar **componentList = NULL;
   IncludedComponents include = NOSPECIALVALUES;

   if (componentString == NULL || *componentString == '\0') {
      g_info("%s: No components included in the ComponentMgr plugin.\n",
             __FUNCTION__);
      return NONECOMPONENTS;
   }

   componentList = g_strsplit(componentString, ",", 0);
   for (i = 0; componentList[i] != NULL; i++ ) {
      g_strstrip(componentList[i]);

      if (strcmp(componentList[i], COMPONENTMGR_ALLCOMPONENTS) == 0) {
         include = ALLCOMPONENTS;
         break;
      }
      if (strcmp(componentList[i], COMPONENTMGR_NONECOMPONENTS) == 0) {
         include = NONECOMPONENTS;
         break;
      }
   }

   g_strfreev(componentList);
   return include;
}


/*
 *****************************************************************************
 * ComponentMgr_UpdateComponentEnableStatus --
 *
 * This functions reads the comma seperated list of components in the included
 * tools.conf configuration and sets the enabled/disabled status for all the
 * components managed by the plugin.
 * It also publishes guestvar guestinfo.vmware.components.available with
 * info of all the components managed by the plugin.
 *
 * @param[in] ctx Tools application context.
 *
 * @return
 *      None.
 *
 * Side effects:
 *      Updates the enabled/disabled status of the all the components managed
 *      by the plugin.
 *
 *****************************************************************************
 */

void
ComponentMgr_UpdateComponentEnableStatus(ToolsAppCtx *ctx) // IN
{
   gchar *listString;
   IncludedComponents included;
   char *token;
   char *context = NULL;

   listString = VMTools_ConfigGetString(ctx->config,
                                        COMPONENTMGR_CONF_GROUPNAME,
                                        COMPONENTMGR_CONF_INCLUDEDCOMPONENTS,
                                        COMPONENTMGR_ALLCOMPONENTS);

   included = ComponentMgrIncludedComponents(listString);
   switch (included) {
      case ALLCOMPONENTS:
         ComponentMgrSetEnabledAllComponents(TRUE);
         goto publishComponents;
      case NONECOMPONENTS:
         ComponentMgrSetEnabledAllComponents(FALSE);
         goto publishComponents;
      default:
         break;
   }

   /*
    * Setting all components to disabled state.
    */
   ComponentMgrSetEnabledAllComponents(FALSE);

   /*
    * Split the comma separated list of included components and individually
    * set the status of each component as TRUE.
    */
   token = strtok_r(listString, ",", &context);
   while (token != NULL) {
      ComponentMgrSetEnabledComponentInfo(token, TRUE);
      token = strtok_r(NULL, ",", &context);
   }

publishComponents:
   g_free(listString);
   ComponentMgrPublishKnownComponents(ctx);
}


/*
 *****************************************************************************
 * ComponentMgrCheckExecuteComponentAction --
 *
 * This function validates the current status of the component against
 * current action for the component and waits for status update counter
 * to reach zero to run a check status operation if the component status
 * and component action are not compliant.
 * If the component action and component status are compliant, it spins off
 * an async check status operation.
 *
 * @param[in] ctx Tools application context.
 * @param[in] componentIndex Index of the component in the global array of
 *                           components.
 * @param[in] action The action that shall be performed on a component.
 *
 * @return
 *      None.
 *
 * Side effects:
 *      Run an asynchronous process to check current status of the component.
 *
 *****************************************************************************
 */

static void
ComponentMgrCheckExecuteComponentAction(ToolsAppCtx *ctx,   // IN
                                        int componentIndex, // IN
                                        const char *action) // IN
{
   char* commandline;
   void (*callbackFunction)(int) = &ComponentMgr_ExecuteComponentAction;
   Action installaction = INVALIDACTION;

   /*
    * It is possible at this stage, an async process for checkstatus or
    * present/absent action may be running for the component. In such a scenario
    * the plugin shall not trigger any other async process.
    */
   ASSERT(components[componentIndex].isEnabled);
   ASSERT(!ComponentMgr_IsAsyncProcessRunning(componentIndex));

   commandline = ComponentMgr_CheckStatusCommandLine(componentIndex);
   if (commandline == NULL) {
      g_info("%s: Unable to construct commandline instruction to run check "
             "status for the component %s\n", __FUNCTION__,
             components[componentIndex].name);
      return;
   }

   /*
    * Add the component to the guest only if it is NOTINSTALLED,
    * INSTALLFAILED or REMOVEFAILED.
    * Remove the component on the guest only if it is INSTALLED,
    * INSTALLFAILED or REMOVEFAILED.
    */
   if((Str_Strcmp(action, COMPONENTMGR_COMPONENTPRESENT) == 0) &&
      (components[componentIndex].status == NOTINSTALLED ||
      components[componentIndex].status == INSTALLFAILED ||
      components[componentIndex].status == REMOVEFAILED)) {
      installaction = PRESENT;
   } else if((Str_Strcmp(action, COMPONENTMGR_COMPONENTABSENT) == 0) &&
             (components[componentIndex].status == INSTALLED ||
             components[componentIndex].status == INSTALLFAILED ||
             components[componentIndex].status == REMOVEFAILED)) {
      installaction = ABSENT;
   } else {
      components[componentIndex].statuscount -= 1;
      if (components[componentIndex].statuscount != 0) {
         /*
          * Status count down for the component has not reached 0
          * come back again in next interval.
          */
         g_debug("%s: Status count down for component %s is %d.\n",
                 __FUNCTION__, components[componentIndex].name,
                 components[componentIndex].statuscount);
         free(commandline);
         return;
      } else {
         /*
          * Status count down has reached 0. We need to call the async
          * check status once and update the last status of the component.
          * Set the callback function for the async check status call NULL,
          * since it's a single check status operation.
          */
         callbackFunction = NULL;
      }
   }

   /*
    * Resetting the value of status count of a component, since the action
    * might have changed or status count down has reached 0.
    */
   components[componentIndex].action = installaction;
   components[componentIndex].statuscount = COMPONENTMGR_CHECK_STATUS_COUNT_DOWN;

   /*
    * Before invoking any action for a component, we need to check the current
    * status for that component. We run the pre configured script with pre
    * configured check status arguments to the script.
    * An async process will be spun off to perform check status of a component
    * with an option of sequenced operation after check status call.
    */
   g_debug("%s: Checking current status of component %s with commandline %s.\n",
           __FUNCTION__, components[componentIndex].name, commandline);
   ComponentMgr_AsynchronousComponentCheckStatus(ctx, commandline,
                                                 componentIndex,
                                                 callbackFunction);
   free(commandline);
}


/*
 *****************************************************************************
 * ComponentMgr_DestroyAsyncProcess --
 *
 * Destroy and free any or all async process running for a component.
 *
 * @return
 *      None.
 *
 * Side  effects:
 *       Kills the async process runing any action for a component instantly.
 *
 *****************************************************************************
 */

void
ComponentMgr_DestroyAsyncProcess()
{
   int i;

   for (i = 0; i < ARRAYSIZE(components); i++) {
      if (components[i].procInfo != NULL) {
         g_debug("%s: Destroying running async process for component %s.\n",
                 __FUNCTION__, components[i].name);
         ComponentMgr_FreeAsyncProc(components[i].procInfo);
      } else {
         g_debug("%s: No async process running for component %s.\n",
                 __FUNCTION__, components[i].name);
      }
   }
}


/*
 *****************************************************************************
 * ComponentMgr_Destroytimers --
 *
 * This function destroys the GSource timers for all components.
 *
 * @return
 *      None.
 *
 * Side  effects:
 *       Destroys the timeout GSource timers for all the components.
 *
 *****************************************************************************
 */

void
ComponentMgr_Destroytimers(void)
{
   int i;

   for (i = 0; i < ARRAYSIZE(components); i++) {
      if (components[i].sourceTimer != NULL) {
         g_debug("%s: Destroying timers for component %s.\n", __FUNCTION__,
                 components[i].name);
         g_source_destroy(components[i].sourceTimer);
         components[i].sourceTimer = NULL;
      } else {
         g_debug("%s: Source timers for component %s has already been "
                 "destroyed.\n", __FUNCTION__, components[i].name);
      }
   }
}


/*
 *****************************************************************************
 * ComponentMgr_UpdateComponentStatus --
 *
 * This function loops through all the enabled components in the plugin and
 * fetches the action for individual components from the guestVar
 * guestinfo./vmware.components.<comp_name>.desiredstate and triggers
 * check status and execute action for a component.
 *
 * @param[in] ctx Tools application context.
 *
 * @return
 *      None.
 *
 * Side effects:
 *      None.
 *
 *****************************************************************************
 */

void
ComponentMgr_UpdateComponentStatus(ToolsAppCtx *ctx) // IN
{
   int i;

   for (i = 0; i < ARRAYSIZE(components); i++) {
      gboolean status;
      char *componentDesiredState = NULL;
      size_t replylen;
      gchar *msg;

     /*
       * Proceed only if the component script is installed and
       * the component is enabled by the plugin.
       */
      if (!components[i].isEnabled) {
         continue;
      }

      msg = g_strdup_printf("%s.%s.%s", COMPONENTMGR_ACTION,
                            components[i].name,
                            COMPONENTMGR_INFODESIREDSTATE);
      /*
       * Fetch the action for a component from the guestVar
       * guestinfo./vmware.components.<comp_name>.desiredstate
       */
      status = ComponentMgr_SendRpc(ctx, msg, &componentDesiredState, &replylen);
      g_free(msg);

      if (!status) {
         g_info("%s: Install action not available for component %s.\n",
                __FUNCTION__, components[i].name);
         vm_free(componentDesiredState);
         componentDesiredState = NULL;
         continue;
      }

      if (componentDesiredState != NULL &&
         (Str_Strcmp(componentDesiredState, COMPONENTMGR_COMPONENTPRESENT) == 0 ||
         Str_Strcmp(componentDesiredState, COMPONENTMGR_COMPONENTABSENT) == 0)) {
         ComponentMgrCheckExecuteComponentAction(ctx, i, componentDesiredState);
      }

      vm_free(componentDesiredState);
      componentDesiredState = NULL;
   }
}
