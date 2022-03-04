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

#ifndef _COMPONENTMGRPlugin_H_
#define _COMPONENTMGRPlugin_H_

/*
 * componentMgrPlugin.h --
 *
 * This file contains macros used by the componentMgr plugin having references
 * for timer related information, guestVar information, component information
 * and configuration related information.
 * Defines functions shared across the componentMgr plugin.
 * Defines states structures to be used to cache and store information related
 * to a component and async process.
 *
 */


#include "conf.h"

#define G_LOG_DOMAIN COMPONENTMGR_CONF_GROUPNAME

#include "vm_basic_defs.h"
#include "vmware/tools/plugin.h"
#include "procMgr.h"

#if defined(_WIN32)
#include <windows.h>
#endif


//********************** Timer Definitions ****************************

/**
 * Default and minimum poll interval for componentMgr in seconds.
 */
#define COMPONENTMGR_DEFAULT_POLL_INTERVAL 180

/**
 * Minimum poll interval for componentMgr in seconds.
 * For development and beta builds the poll-interval can be configured
 * lower than the default poll-interval.
 */
#ifdef VMX86_DEBUG
#define COMPONENTMGR_MIN_POLL_INTERVAL 5
#else
#define COMPONENTMGR_MIN_POLL_INTERVAL COMPONENTMGR_DEFAULT_POLL_INTERVAL
#endif

/*
 * Poll interval between 2 consecutive check status operation in seconds.
 */
#define COMPONENTMGR_ASYNC_CHECK_STATUS_POLL_INTERVAL 1

/*
 * Max time in seconds after which the async process running check status
 * command will be terminated.
 */
#define COMPONENTMGR_ASYNC_CHECK_STATUS_TERMINATE_PERIOD 15

/*
 * Poll interval for waiting on the async process runnning the action for a
 * component in seconds.
 */
#define COMPONENTMGR_ASYNCPROCESS_POLL_INTERVAL 5

/*
 * The wait period after which the async proces needs to be killed for a
 * component in seconds.
 */
#define COMPONENTMGR_ASYNCPROCESS_TERMINATE_PERIOD 600

/*
 * The amount of times the check status operation needs to wait before any
 * change in the guetsVar to trigger another checkstatus opeartion.
 */
#define COMPONENTMGR_CHECK_STATUS_COUNT_DOWN 10

//********************** Component Action Definitions *********************

/**
 * Defines check status action on the component.
 */
#define COMPONENTMGR_COMPONENTCHECKSTATUS "checkstatus"

/**
 * Defines an invalid action on the component.
 */
#define COMPONENTMGR_COMPONENINVALIDACTION "invalidaction"

/**
 * Defines present action for a component to be installed on a system.
 */
#define COMPONENTMGR_COMPONENTPRESENT "present"

/**
 * Defines absent action for a component to be removed from a system.
 */
#define COMPONENTMGR_COMPONENTABSENT "absent"

//********************** Guest Variable Definitions *********************

/**
 * Defines argument to publish installed and enabled components.
 */
#define COMPONENTMGR_INFOAVAILABLE "available"

/**
 * Defines argument to publish last status of a particular component.
 */
#define COMPONENTMGR_INFOLASTSTATUS "laststatus"

/**
 * Defines action to be taken on a component.
 * guestinfo./vmware.components.<comp_name>.desiredstate
 */
#define COMPONENTMGR_INFODESIREDSTATE "desiredstate"

/*
 * GuestVar prefix string to fetch the action required for a component.
 */
#define COMPONENTMGR_ACTION "info-get guestinfo./vmware.components"

/**
 * String to set informational guestVar exposed by the plugin.
 */
#define COMPONENTMGR_PUBLISH_COMPONENTS "info-set guestinfo.vmware.components"

//********************** Component Definitions *********************

/**
 * Defines the directory for the plugin to host the scripts.
 */
#define COMPONENTMGR_DIRECTORY "componentMgr"

/**
 * Defines none to indicate no component is managed by the plugin.
 */
#define COMPONENTMGR_NONECOMPONENTS "none"

/**
 * Define all the names of components under this section.
 */
#define SALT_MINION "salt_minion"

/**
 * Defines a config all in included to indicate all the components are
 * managed by the plugin.
 */
#define COMPONENTMGR_ALLCOMPONENTS "all"

/*
 * The included param in the tools.conf contains comma seperated list
 * of components and can have special values.
 * Defines various special values present in the included tools.conf param.
 */

typedef enum IncludedComponents
{
  ALLCOMPONENTS,
  NONECOMPONENTS,
  NOSPECIALVALUES
} IncludedComponents;

//*******************************************************************

/*
 * Installation status of the components managed by the componentMgr plugin.
 * The status for each component will be updated based on the exit code
 * returned by the script executing check status operation.
 */

typedef enum InstallStatus
{
   INSTALLED = 100,         /* The component is installed on the guest OS. */
   INSTALLING,              /* The component is being installed on the guest
                               OS. */
   NOTINSTALLED,            /* The component is not installed on the guest OS.
                             */
   INSTALLFAILED,           /* The component install failed on the guest OS. */
   REMOVING,                /* The component is being removed on the guest OS.
                             */
   REMOVEFAILED,            /* The component remove failed on the guest OS. */
   SCRIPTFAILED = 126,      /* The component script failed for some reason. */
   SCRIPTTERMINATED = 130   /* The component script terminated for some reason.
                             */
} InstallStatus;


/*
 * Actions currently supported by the componentMgr plugin for the known and
 * enabled components.
 */

typedef enum Action
{
   PRESENT,      /* The action adds/installs the components on the guest. */
   ABSENT,       /* The action removes/uninstalls the components on the guest.*/
   CHECKSTATUS,  /* The action calls the preconfigured script to check the
                    current status of the component. */
   INVALIDACTION /* Action not recongnised by the plugin. */
} Action;


/*
 * Structure to store information about the asynchronous process being run
 * for a particular component.
 */

typedef struct AsyncProcessInfo {
   ProcMgr_AsyncProc *asyncProc; /* ProcMgr_AsyncProc structure consisting of
                                    the process data running an action on the
                                    component. */
   ToolsAppCtx *ctx;             /* Tools application context. */
   int backoffTimer;             /* Backoff timer to wait until timeout
                                    to kill the asynchronous process. */
   int componentIndex;           /* The index of the component in the global
                                    array of components. */
   void (*callbackFunction)(int componentIndex); /* A callback function to
                                                    sequence a new operation
                                                  */
} AsyncProcessInfo;


/*
 * This structure contains all the information related to all the components
 * managed by the plugin. The component states is maintained in this structure.
 */

typedef struct ComponentInfo
{
   const char *name;     /* The name of the component. */
   gboolean isEnabled;   /* Component enabled/disabled by the plugin. */
   InstallStatus status; /* Contains current status of the component. */
   GSource *sourceTimer; /* A GSource timer for async process monitoring running
                            an operation for a component. */
   AsyncProcessInfo *procInfo; /* A structure to store information about the
                                * current running async process for a component.
                                */
   int statuscount;      /* A counter value to store max number of times to
                            wait before starting another checkstatus opeartion
                          */
   Action action;        /* Contains information about the action to be
                            performed on a component. */
} ComponentInfo;


void
ComponentMgrUpdateComponentEnableStatus(ToolsAppCtx *ctx);


void
ComponentMgr_UpdateComponentStatus(ToolsAppCtx *ctx);


gboolean
ComponentMgr_SendRpc(ToolsAppCtx *ctx,
                     const char *guestInfoCmd,
                     char **outBuffer,
                     size_t *outBufferLen);


void
ComponentMgr_Destroytimers();


const char*
ComponentMgr_GetComponentInstallStatus(InstallStatus installStatus);


const char*
ComponentMgr_GetComponentAction(Action action);


void
ComponentMgr_AsynchronousComponentActionStart(ToolsAppCtx *ctx,
                                              const char *commandline,
                                              int componetIndex);


void
ComponentMgr_SetStatusComponentInfo(ToolsAppCtx *ctx,
                                    int exitCode,
                                    int componentIndex);


char *
ComponentMgr_CheckStatusCommandLine(int componentIndex);


void
ComponentMgr_UpdateComponentEnableStatus(ToolsAppCtx *ctx);


void
ComponentMgr_SetComponentGSourceTimer(GSource *componentTimer,
                                      int componentIndex);

void
ComponentMgr_ResetComponentGSourceTimer(int componentIndex);


void
ComponentMgr_ExecuteComponentAction(int componentIndex);


void
ComponentMgr_AsynchronousComponentCheckStatus(ToolsAppCtx *ctx,
                                              const char *commandline,
                                              int componentIndex,
                                              void (*callback)(int compIndex));


ToolsAppCtx*
ComponentMgr_GetToolsAppCtx();


const char*
ComponentMgr_GetIncludedComponents(IncludedComponents pos);


void
ComponentMgr_SetComponentAsyncProcInfo(AsyncProcessInfo *asyncProcInfo,
                                       int componentIndex);


void
ComponentMgr_ResetComponentAsyncProcInfo(int componentIndex);


gboolean
ComponentMgr_IsAsyncProcessRunning(int componentIndex);


const char*
ComponentMgr_GetComponentName(int componentIndex);


void
ComponentMgr_FreeAsyncProc(AsyncProcessInfo *procInfo);


void
ComponentMgr_DestroyAsyncProcess();


void
ComponentMgr_PublishAvailableComponents(ToolsAppCtx *ctx,
                                        const char *components);


gboolean
ComponentMgr_CheckAnyAsyncProcessRunning();
#endif /* _ComponentMgrPlugin_H_ */
