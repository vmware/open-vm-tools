/*********************************************************
 * Copyright (C) 2011-2019 VMware, Inc. All rights reserved.
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
 * @file main.c --
 *
 *    Entry point for the GuestAuth service.
 */

#include <locale.h>
#include "serviceInt.h"
#include "service.h"
#include "buildNumber.h"
#ifdef _WIN32
#include <tchar.h>
#include "winCoreDump.h"
#endif

#ifdef _WIN32

/*
 * Turn this on to build as a Windows service.  Note that this
 * affects the cmdline arg parsing, and the default no-argument case
 * runs as a Window service.
 * This means to be run in a non-service mode, you must pass a "-d' flag.
 *
 * Most of the service code was stolen from bora/apps/vmauthd/authdService.c
 */

#define SUPPORT_WIN_SERVICE 1

/*
 * XXX
 *
 * Note that to install the service, it should be executed with its
 * complete path name, since the (un)register code just uses argv[0]
 * as the path to the executable.  A relative path will just confuse
 * the service control manager.
 */


#if SUPPORT_WIN_SERVICE

/*
 * Details about the Windows service
 */
#define VGAUTH_DISPLAY_NAME "VMware Alias Manager and Ticket Service"
#define VGAUTH_DESCRIPTION "Alias Manager and Ticket Service"

static SERVICE_STATUS VGAuthServiceStatus = { 0 };
static SERVICE_STATUS_HANDLE VGAuthServiceStatusHandle = 0;
static HANDLE hServiceThread = NULL;
static HANDLE hServiceQuitEvent = NULL;
#endif

#endif   // _WIN32

#ifndef _WIN32
#define USE_POSIX_SERVICE 1

#include <sys/types.h>
#include <unistd.h>

static gboolean isRunningAsService = FALSE;
static char *pidFileName = "/var/run/vmware/vgauthsvclog_pid.txt";

#endif


/*
 ******************************************************************************
 * ServiceHelp --                                                        */ /**
 *
 * Dump some simple help.
 *
 ******************************************************************************
 */

static void
ServiceHelp(char *arg)
{
   printf("Usage: %s [OPTION]\n", arg);
   printf("Service to support SAML token and ticketing authentication"
         " for VMware products.\n\n");
#ifdef _WIN32
   printf("\t-r\tRegister as a Windows Service.\n");
   printf("\t-u\tUnregister as a Windows Service.\n");
   printf("\t-d\tRun as a normal program, sending logging to stdio.\n");
   printf("\t-s\tRun as a normal program, sending logging to a file.\n");
#else
#if USE_POSIX_SERVICE
   printf("\t-k\tKill the running instance that was started as a daemon.\n");
   printf("\t-s\tRun in daemon mode.\n");
   printf("\t-b\tRun in background mode, using a pid lock file.\n");
#endif
#endif
   printf("\t-h\tDisplay this help and exit.\n");
}


/*
 ******************************************************************************
 * ServiceStartAndRun --                                               */ /**
 *
 * Does the work to start up and run the service.  Never returns.
 *
 ******************************************************************************
 */

static void
ServiceStartAndRun(void)
{
   VGAuthError err;
   ServiceConnection *publicConn;

   gboolean auditSuccess = Pref_GetBool(gPrefs,
                                        VGAUTH_PREF_AUDIT_SUCCESS,
                                        VGAUTH_PREF_GROUP_NAME_AUDIT,
                                        TRUE);
   gchar *msgCatalog = Pref_GetString(gPrefs,
                                      VGAUTH_PREF_LOCALIZATION_DIR,
                                      VGAUTH_PREF_GROUP_NAME_LOCALIZATION,
                                      VGAUTH_PREF_DEFAULT_LOCALIZATION_CATALOG);

   setlocale(LC_ALL, "");
   I18n_BindTextDomain(VMW_TEXT_DOMAIN, NULL, msgCatalog);
   g_free(msgCatalog);

   Audit_Init(VGAUTH_SERVICE_NAME, auditSuccess);

   Log("INIT SERVICE\n");

   VMXLog_Init();
   VMXLog_Log(VMXLOG_LEVEL_INFO, "%s %s starting up",
              VGAUTH_SERVICE_NAME, BUILD_NUMBER);

#ifdef _WIN32
   if (ServiceOldInstanceExists()) {
      Warning("%s: another instance is running; exiting\n", __FUNCTION__);
      exit(-1);
   }
#endif

   err = ServiceAliasInitAliasStore();
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to init alias store; exiting\n", __FUNCTION__);
      exit(-1);
   }

   err = ServiceInitTickets();
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to init tickets; exiting\n", __FUNCTION__);
      exit(-1);
   }

   err = ServiceInitVerify();
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to init verification; exiting\n", __FUNCTION__);
      exit(-1);
   }

   err = ServiceRegisterIOFunctions(ServiceIOStartListen, ServiceStopIO);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to register IO functions; exiting\n", __FUNCTION__);
      exit(-1);
   }


   err = ServiceCreatePublicConnection(&publicConn);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to create public listen sock; exiting\n", __FUNCTION__);
      exit(-1);
   }

   err = ServiceIOStartListen(publicConn);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to listen on public sock; exiting\n", __FUNCTION__);
      exit(-1);
   }

   err = ServiceIOPrepareMainLoop();
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to set up main loop; exiting\n", __FUNCTION__);
      exit(-1);
   }

   /*
    * This never comes back
    */
   Log("BEGIN SERVICE\n");
   err = ServiceIOMainLoop();
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to run main loop; exiting\n", __FUNCTION__);
      exit(-1);
   }

   // NOTREACHED
   return;
}


#if SUPPORT_WIN_SERVICE

/*
 ******************************************************************************
 * ServiceDoRegisterService --                                           */ /**
 *
 * (Un)registers as a Windows service.
 * Expects the path to be absolute, with the full command name.
 *
 * @param[in]   path       The path to the executable.
 * @param[in]   doRegister Set if the service is being registered.
 *
 ******************************************************************************
 */

static void
ServiceDoRegisterService(gchar *path,
                         gboolean doRegister)
{
   gboolean bRet;
   gchar *errString;

   /*
    * XXX may want to add code to turn a non-absolute path into one,
    * and tack on ".exe" if needed.  Since in theory this code
    * will only be run by an installer which knows the full path,
    * leaving it out for now.
    */

   bRet = ServiceRegisterService(doRegister,
                                 VGAUTH_SERVICE_NAME,
                                 VGAUTH_DISPLAY_NAME,
                                 VGAUTH_DESCRIPTION,
                                 path,
                                 &errString);
   if (!bRet) {
      fprintf(stderr, "%s: %s\n", path, errString);
   } else {
      if (doRegister) {
         printf("Successfully registered %s.\n", VGAUTH_DISPLAY_NAME);
      } else {
         printf("Successfully unregistered %s.\n", VGAUTH_DISPLAY_NAME);
      }
   }

   g_free(errString);
}


/*
 ******************************************************************************
 * ServiceCtrlHandler --                                                 */ /**
 *
 * @param[in]   Opcode   The opcode from the service control manager.
 *
 * Handler for Windows service control messages.
 ******************************************************************************
 */

VOID WINAPI
ServiceCtrlHandler(DWORD opCode)
{
   DWORD status;

   switch(opCode) {
   case SERVICE_CONTROL_PAUSE:
      /*
       * Do whatever it takes to pause here.
       *
       * XXX we don't actually pause any operations...
       */
      VGAuthServiceStatus.dwCurrentState = SERVICE_PAUSED;
      Log("Service Paused.\n");
      break;

   case SERVICE_CONTROL_CONTINUE:
      // Do whatever it takes to continue here.
      VGAuthServiceStatus.dwCurrentState = SERVICE_RUNNING;
      Log("Service Continuing.\n");
      break;

   case SERVICE_CONTROL_STOP:
      // Do whatever it takes to stop here.
      VGAuthServiceStatus.dwWin32ExitCode = 0;
      VGAuthServiceStatus.dwCurrentState  = SERVICE_STOP_PENDING;
      VGAuthServiceStatus.dwCheckPoint    = 0;
      VGAuthServiceStatus.dwWaitHint      = 0;

      if (!SetServiceStatus(VGAuthServiceStatusHandle, &VGAuthServiceStatus)) {
         status = GetLastError();
         VGAUTH_LOG_ERR_WIN("SetServiceStatus failed while stopping\n");
         return;
      }

      if (hServiceThread != NULL) {
         SetEvent(hServiceQuitEvent);
         if (WaitForSingleObject(hServiceThread, 15000) != WAIT_OBJECT_0) {
            Log("Forced to clobber service thread\n");
            TerminateThread(hServiceThread, 0);
         }
         CloseHandle(hServiceThread);
         hServiceThread = NULL;
         CloseHandle(hServiceQuitEvent);
         hServiceQuitEvent = NULL;
      }

      VGAuthServiceStatus.dwCurrentState = SERVICE_STOPPED;
      Log("Service Stopped.\n");
      break;

   case SERVICE_CONTROL_INTERROGATE:
      Log("Service being interrogated....\n");
      break;

   default:
      Warning("Unknown service opcode %d\n", opCode);
      break;
   }

   // Send current status.
   if (!SetServiceStatus(VGAuthServiceStatusHandle, &VGAuthServiceStatus)) {
      VGAUTH_LOG_ERR_WIN("SetServiceStatus failed.\n");
   }

   return;
}


/*
 ******************************************************************************
 * ServiceStartServiceThread --                                          */ /**
 *
 * Starts a thread to do the real work.
 *
 * @return TRUE on success
 ******************************************************************************
 */

static gboolean
ServiceStartServiceThread(void)
{
   VGAuthError err;

   if (!(hServiceQuitEvent = CreateEvent(NULL, FALSE, FALSE, NULL))) {
      VGAUTH_LOG_ERR_WIN("Failed to create shutdown event");
      return FALSE;
   }

   hServiceThread = CreateThread(NULL, 0,
                                 (LPTHREAD_START_ROUTINE) ServiceStartAndRun,
                                 NULL, 0, NULL);

   if (NULL == hServiceThread) {
      VGAUTH_LOG_ERR_WIN("Failed to start service thread\n");
      return FALSE;
   }

   err = ServiceIORegisterQuitEvent(hServiceQuitEvent);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to register quit event\n");
      return FALSE;
   }

   return TRUE;
}


/*
 ******************************************************************************
 * ServiceServiceStart --                                                */ /**
 *
 * Service entry point.
 *
 * @param[in]   argc       Number of args (unused).
 * @param[in]   argv       Arguments (unused).
 *
 ******************************************************************************
 */

VOID WINAPI
ServiceServiceStart(DWORD argc,
                    LPTSTR *argv)
{
   BOOL bRet;
   DWORD status;

   VGAuthServiceStatus.dwServiceType        = SERVICE_WIN32;
   VGAuthServiceStatus.dwCurrentState       = SERVICE_START_PENDING;
   VGAuthServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP;

   VGAuthServiceStatusHandle = RegisterServiceCtrlHandler(_T(VGAUTH_SERVICE_NAME),
                                                           ServiceCtrlHandler);

   if (0 == VGAuthServiceStatusHandle) {
      Warning("%s: RegisterServiceCtrlHandler failed %d\n",
              __FUNCTION__, GetLastError());
      return;
   }

   /*
    * Initialization code goes here.
    */

   bRet = ServiceStartServiceThread();

   /*
    * Handle any error
    */
   if (!bRet) {
      VGAuthServiceStatus.dwCurrentState       = SERVICE_STOPPED;
      VGAuthServiceStatus.dwCheckPoint         = 0;
      VGAuthServiceStatus.dwWaitHint           = 0;
      VGAuthServiceStatus.dwWin32ExitCode      = ERROR_SERVICE_SPECIFIC_ERROR;
      VGAuthServiceStatus.dwServiceSpecificExitCode = -1;

      if (!SetServiceStatus(VGAuthServiceStatusHandle,
                            &VGAuthServiceStatus)) {
         Warning("%s: SetServiceStatus error on failure %ld\n",
                 __FUNCTION__, GetLastError());
      }
      return;
   }

   /*
    * Initialization complete - report running status.
    */
   VGAuthServiceStatus.dwCurrentState       = SERVICE_RUNNING;
   VGAuthServiceStatus.dwCheckPoint         = 0;
   VGAuthServiceStatus.dwWaitHint           = 0;

   if (!SetServiceStatus(VGAuthServiceStatusHandle, &VGAuthServiceStatus)) {
      status = GetLastError();
      Warning("%s: SetServiceStatus error %ld\n", __FUNCTION__, status);
   }

#if 0
   /*
    * Fun race here -- if we spew too soon, the debug system isn't properly
    * set up and the Log thinks its recursed and blows up.
    * A Sleep() avoids this, but since we don't really need this noise,
    * turn it off.  Leave this warning in case someone wants to turn it back on.
    */
   Log("Service Started.\n");
#endif

   return;
}


/*
 ******************************************************************************
 * ServiceRunAsService --                                                */ /**
 *
 * Starts as a Windows service.
 *
 ******************************************************************************
 */

void
ServiceRunAsService(void)
{
   gboolean haveDebugConsole = FALSE;
   SERVICE_TABLE_ENTRY DispatchTable[] = {
      { _T(VGAUTH_SERVICE_NAME), ServiceServiceStart},
      { NULL,                     NULL              }
   };

#ifdef VMX86_DEBUG
   /*
    * In devel builds, create a console so warnings are visible.  NB the
    * console won't be visible unless you also check "allow service to
    * interact with desktop" in the Services manager.  I tried hard, and
    * failed, to get the console to show up on the interactive winsta/desk
    * after we move ourselves there -- we're able to create GUI windows
    * fine -- I can only conclude that Windows is broken in this regard.
    * Google for "SetProcessWindowStation AllocConsole" and read the
    * Apache source, or see me, for an amusing story about this.  -- mginzton
    */

   haveDebugConsole = ServiceInitStdioConsole();
#endif

   /*
    * XXX: We should be able to get everything automatically redirected
    * to the new console. Try skip the InitLogging code... TBD
    * When we're in service mode, make sure the debug isn't lost.
    */
   Service_InitLogging(haveDebugConsole, FALSE);

   if (!StartServiceCtrlDispatcher(DispatchTable)) {
      Warning("%s: StartServiceCtrlDispatcher error = %d\n",
              __FUNCTION__, GetLastError());
   }
}
#endif   // SUPPORT_WIN_SERVICE


/*
 ******************************************************************************
 * main --                                                              */ /**
 *
 * The one, the only: main(). Runs the GuestAuth service.
 *
 * @param[in]  argc        Number of command line arguments.
 * @param[in]  argv        The command line arguments.
 *
 * @return 0 if the service ran successfully, -1 if there was an error during
 *         start-up or execution.
 *
 ******************************************************************************
 */

int
main(int argc,
     char *argv[])
{
#ifdef _WIN32
   WinUtil_EnableSafePathSearching();
#endif

   gPrefs = Pref_Init(VGAUTH_PREF_CONFIG_FILENAME);

   /*
    * Determine where the service is running from, so resources can
    * be found relative to it.
    */
   if (!g_path_is_absolute(argv[0])) {
      gchar *abs = g_find_program_in_path(argv[0]);
      if (abs == NULL || g_strcmp0(abs, argv[0]) == 0) {
         gchar *cwd = g_get_current_dir();
         g_free(abs);
         abs = g_build_filename(cwd, argv[0], NULL);
         g_free(cwd);
      }
      gInstallDir = g_path_get_dirname(abs);
      g_free(abs);
   } else {
      gInstallDir = g_path_get_dirname(argv[0]);
   }

#ifdef _WIN32
#if SUPPORT_WIN_SERVICE

   /*
    * This is the path for the service control manager.
    */
   if (argc == 1) {
      ServiceRunAsService();
      return 0;
   } else if (argc == 2) {
      // register
      if (g_strcmp0(argv[1], "-r") == 0) {
         ServiceDoRegisterService(argv[0], TRUE);
         return 0;
      // unregister
      } else if (g_strcmp0(argv[1], "-u") == 0) {
         ServiceDoRegisterService(argv[0], FALSE);
         return 0;
      // run as a cmdline app for debugging
      } else if (g_strcmp0(argv[1], "-d") == 0) {
         Service_SetLogOnStdout(TRUE);
         Service_InitLogging(FALSE, FALSE);
         ServiceStartAndRun();
         return 0;
       // run on cmdline, using log file
      } else if (g_strcmp0(argv[1], "-s") == 0) {
         Service_InitLogging(FALSE, FALSE);
         ServiceStartAndRun();
         return 0;
      } else if (g_strcmp0(argv[1], "-h") == 0) {
         ServiceHelp(argv[0]);
         return 0;
      }
   }

#else
   Service_SetLogOnStdout(TRUE);
   Service_InitLogging(FALSE, FALSE);
   ServiceStartAndRun();
#endif
#else // !_WIN32

#if USE_POSIX_SERVICE
   /*
    * Parse arguments.
    *
    * "-b" tells it to run as a daemon.
    * "-s" tells it to run in service mode (logging to a file).
    * "-k" tells it to kill itself.
    *
    * When running as a daemon, we restart, except with -b changed
    * to -s so we properly log to a file.
    *
    * This code assumes the only arguments supported are "-b" and "-k".
    * The replacement of "-b" before calling ServiceDamonize()
    * will need work if that changes.
    */

   if (argc > 1) {
      if (g_strcmp0(argv[1], "-k") == 0) {            // kill mode
         if (!ServiceSuicide(pidFileName)) {
            exit(-1);
         } else {
            exit(0);
         }
      } else if (g_strcmp0(argv[1], "-s") == 0) {     // service mode
         isRunningAsService = TRUE;
         Service_InitLogging(FALSE, FALSE);
      } else if (g_strcmp0(argv[1], "-b") == 0) {     // background mode
         Service_InitLogging(FALSE, FALSE);
         /*
          * We have to remove this flag to prevent an infinite loop.
          */
         argv[1] = g_strdup("-s");
         if (!ServiceDaemonize(argv[0],
                               argv,
                               SERVICE_DAEMONIZE_LOCKPID,
                               pidFileName)) {
            Warning("%s: failed to daemonize\n", __FUNCTION__);
            return -1;
         }
         // NOTREACHED
         return 0;
      } else if (g_strcmp0(argv[1], "-h") == 0) {     // help
         ServiceHelp(argv[0]);
         return 0;
      } else {
         Warning("%s: unrecognized args\n", __FUNCTION__);
      }
   } else {
      /* The foreground mode */
      Service_SetLogOnStdout(TRUE);
      Service_InitLogging(FALSE, FALSE);
   }
#endif   // USE_POSIX_SERVICE
   ServiceSetSignalHandlers();
   ServiceStartAndRun();

#endif   // !_WIN32

   return 0;
}
