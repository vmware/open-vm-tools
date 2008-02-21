/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/


/*
 * guestApp.c --
 *
 *    Utility functions common to all guest applications
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "vmware.h"
#include "vm_app.h"
#include "vm_version.h"
#include "vm_tools_version.h"
#include "guestApp.h"
#include "hgfs.h"
#include "backdoor.h"
#include "backdoor_def.h"
#include "rpcout.h"
#include "debug.h"
#include "strutil.h"
#include "str.h"
#include "dictll.h"
#include "msg.h"
#include "file.h"
#include "dynbuf.h"
#include "vmstdio.h"
#include "codeset.h"
#include "productState.h"

#if !defined(N_PLAT_NLM)
# include "hgfs.h"
# include "cpName.h"
# include "cpNameUtil.h"
#endif

#ifdef _MSC_VER
#include <windows.h>
#include <shlobj.h>
#endif


/*
 * An option name/value pair stored locally in the guest app.
 */
typedef struct GuestApp_DictEntry {
   char *name;
   char *value;
   char *defaultVal;
   struct GuestApp_DictEntry *next;
} GuestApp_DictEntry;

/*
 * A name/value dictionary.
 */
struct GuestApp_Dict {
   GuestApp_DictEntry head;
   int64 fileModTime;
   char *fileName;
};

/* Function pointer, used in GuestApp_GetConfPath. */
#if defined(_WIN32)
typedef HRESULT (WINAPI *PSHGETFOLDERPATH)(HWND, int, HANDLE, DWORD, LPTSTR);
static PSHGETFOLDERPATH pfnSHGetFolderPath = NULL;
#endif

Bool runningInForeignVM = FALSE;


/*
 *----------------------------------------------------------------------
 *
 * GuestAppNewDictEntry --
 *
 *      Allocate & add a new entry to the dict list.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
GuestAppNewDictEntry(GuestApp_DictEntry **pCur, // IN/OUT: add after this entry
                     const char *name,          // IN
                     const char *value,         // IN
                     const char *defaultVal)    // IN
{
   GuestApp_DictEntry *p;

   ASSERT(pCur);

   /* Not found so allocate & init a new entry */
   p = (GuestApp_DictEntry *) malloc(sizeof(GuestApp_DictEntry));
   ASSERT_NOT_IMPLEMENTED(p);

   if (name) {
      p->name = strdup(name);
   } else {
      p->name = NULL;
   }

   if (value) {
      p->value = strdup(value);
   } else {
      p->value = NULL;
   }

   if (defaultVal) {
      p->defaultVal = strdup(defaultVal);
   } else {
      p->defaultVal = NULL;
   }

   p->next = *pCur;
   *pCur = p;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_ConstructDict --
 *
 *      Construct a new dict. A dict is used to store name/value pairs
 *      within the guest app for later lookup.
 *
 * Results:
 *      The new dict.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

GuestApp_Dict *
GuestApp_ConstructDict(char *fileName) // IN: you give up your right to free this
{
   GuestApp_Dict *dict;

   dict = (GuestApp_Dict *) malloc(sizeof(GuestApp_Dict));
   ASSERT_NOT_IMPLEMENTED(dict);

   dict->head.next = NULL;

   dict->fileName = fileName;

   return dict;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_SetDictEntry --
 *
 *      Set the value of a dict entry.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
GuestApp_SetDictEntry(GuestApp_Dict *dict, // IN/OUT
                      const char *name,    // IN
                      const char *value)   // IN
{
   GuestApp_DictEntry *p;

   ASSERT(dict);
   ASSERT(name);
   ASSERT(value);

   for (p = dict->head.next; p; p = p->next) {
      if (strcmp(p->name, name) == 0) {
         if (strcmp(p->value, value) != 0) {
            Debug("Changing dict entry: %s = %s -> %s\n", p->name, p->value, value);

            free(p->value);
            p->value = strdup(value);
         }

         return;
      }
   }

   Debug("Adding dict entry: %s = %s\n", name, value);

   /* Not found so create a new entry right after the dummy */
   GuestAppNewDictEntry(&dict->head.next, name, value, NULL);
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_SetDictEntryDefault --
 *
 *      Set the default value of a dict entry.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
GuestApp_SetDictEntryDefault(GuestApp_Dict *dict,    // IN/OUT
                             const char *name,       // IN
                             const char *defaultVal) // IN
{
   GuestApp_DictEntry *p;

   ASSERT(dict);
   ASSERT(name);
   ASSERT(defaultVal);

   for (p = dict->head.next; p; p = p->next) {
      if (strcmp(p->name, name) == 0) {
         Debug("Changing dict entry default: %s = %s -> %s\n",
               p->name, p->defaultVal, defaultVal);

         free(p->defaultVal);
         p->defaultVal = strdup(defaultVal);
         free(p->value);
         p->value = strdup(defaultVal);
         return;
      }
   }

   Debug("Adding dict entry default: %s = %s\n", name, defaultVal);

   /* Not found so create a new entry right after the dummy */
   GuestAppNewDictEntry(&dict->head.next, name, defaultVal, defaultVal);
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_GetDictEntry --
 *
 *      Get the value of a dict entry.
 *
 * Results:
 *      The value or NULL if not found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

const char *
GuestApp_GetDictEntry(GuestApp_Dict *dict, // IN
                      const char *name)    // IN
{
   GuestApp_DictEntry *p;
   const char *value = NULL;

   ASSERT(dict);
   ASSERT(name);

   for (p = dict->head.next; p; p = p->next) {
      //Debug("-Found dict entry %s = %s while looking for %s\n", p->name, p->value, name);
      if (strcmp(p->name, name) == 0) {
         value = p->value;
         break;
      }
   }

   return value;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_GetDictEntryDefault --
 *
 *      Get the default value of a dict entry.
 *
 * Results:
 *      The value or NULL if not found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

const char *
GuestApp_GetDictEntryDefault(GuestApp_Dict *dict, // IN
                             const char *name)    // IN
{
   GuestApp_DictEntry *p;
   const char *value = NULL;

   ASSERT(dict);
   ASSERT(name);

   for (p = dict->head.next; p; p = p->next) {
      //Debug("-Found dict entry default %s = %s while looking for %s\n", p->name, p->defaultVal, name);
      if (strcmp(p->name, name) == 0) {
         value = p->defaultVal;
         break;
      }
   }

   return value;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_GetDictEntryInt --
 *
 *      Get a dict entry's value & convert it to an int.
 *
 * Results:
 *      TRUE on success
 *      FALSE if:
 *        -the value was NULL
 *        -the value was not an int
 *
 * Side effects:
 *	Warns if the value is NULL or not an int.
 *
 *----------------------------------------------------------------------
 */

Bool
GuestApp_GetDictEntryInt(GuestApp_Dict *dict, // IN
                         const char *name,    // IN
                         int32 *out)          // OUT
{
   const char *value;
   int32 intVal;

   ASSERT(dict);
   ASSERT(name);
   ASSERT(out);

   value = GuestApp_GetDictEntry(dict, name);
   if (!value) {
      Warning("GuestApp: no value for option '%s'\n", name);

      return FALSE;
   }

   if (!StrUtil_StrToInt(&intVal, value)) {
      Warning("GuestApp: invalid int value for option '%s'; "
         "value='%s'\n", name, value);

      return FALSE;
   }

   *out = intVal;
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_GetDictEntryBool --
 *
 *      Get a dict entry's value & convert it to an Bool.
 *
 * Results:
 *      Returns TRUE is the dict entry is a case-insensitive match
 *      to "TRUE", FALSE otherwise. If there is no entry, for
 *      "name" or dict is NULL, return FALSE as well.
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------
 */

Bool
GuestApp_GetDictEntryBool(GuestApp_Dict *dict, // IN
                          const char *name)    // IN
{
   const char *value;

   ASSERT(dict);

   value = GuestApp_GetDictEntry(dict, name);
   if (!value) {
      return FALSE;
   }

#if  (defined N_PLAT_NLM || defined _WIN32)
   return (stricmp(value, "TRUE") == 0);
#else
   return (strcasecmp(value, "TRUE") == 0);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * GetApp_FreeDict --
 *
 *      Free the dict.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
GuestApp_FreeDict(GuestApp_Dict *dict) // IN/OUT
{
   ASSERT(dict);

   while (dict->head.next) {
      GuestApp_DictEntry *p = dict->head.next;

      dict->head.next = p->next;

      free(p->name);
      free(p->value);
      free(p->defaultVal);
      free(p);
   }

   free(dict->fileName);

   free(dict);
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_WasDictFileChanged --
 *
 *      Has the dict file been changed since the last time we loaded
 *      or wrote it?
 *
 * Results:
 *      TRUE if it has; FALSE if it hasn't
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
GuestApp_WasDictFileChanged(GuestApp_Dict *dict)
{
   ASSERT(dict);
   ASSERT(dict->fileName);

   return (File_GetModTime(dict->fileName) > dict->fileModTime);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_OldGetOptions --
 *
 *    Retrieve the tools options from VMware using the old (deprecated) method.
 *
 * Return value:
 *    The tools options
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

uint32
GuestApp_OldGetOptions(void)
{
   Backdoor_proto bp;

   if (runningInForeignVM) {
      return(0);
   } else {
      Debug("Retrieving tools options (old)\n");

      bp.in.cx.halfs.low = BDOOR_CMD_GETGUIOPTIONS;
      Backdoor(&bp);
      return bp.out.ax.word;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_OldSetOptions --
 *
 *    Send the tools options to VMware using the old (deprecated) method.
 *
 * Return value:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
GuestApp_OldSetOptions(uint32 options) // IN
{
   Backdoor_proto bp;

   if (!runningInForeignVM) {
      Debug("Setting tools options (old)\n");

      bp.in.cx.halfs.low = BDOOR_CMD_SETGUIOPTIONS;
      bp.in.size = options;
      Backdoor(&bp);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_SetOptionInVMX --
 *
 *      Send an option's value to VMware.
 *      NOTE: vmware should have unified loop capability to accept
 *            this option.
 *
 * Results:
 *      TRUE:  success
 *      FALSE: failure to due an RpcOut error or an invalid
 *             currentVal
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
GuestApp_SetOptionInVMX(const char *option,     // IN
                        const char *currentVal, // IN
                        const char *newVal)     // IN
{
   return RpcOut_sendOne(NULL, NULL, "vmx.set_option %s %s %s",
                         option, currentVal, newVal);
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_Log --
 *
 *      Send a string to VMware's log via the backdoor.
 *
 * Results:
 *      TRUE/FALSE depending on whether the RPCI succeeded or failed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
GuestApp_Log(const char *s) // IN
{
   return RpcOut_sendOne(NULL, NULL, "log %s: %s", TOOLS_DAEMON_NAME, s);
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_GetUnifiedLoopCap --
 *
 *    Check to see if the VMX supports TCLO as the medium for
 *    updating all the config options.
 *
 * Return value:
 *    TRUE:  if VMware supports the unified TCLO loop (i.e. can send us options

 *           via TCLO.
 *    FALSE: if VMware doesn't support the unified loop or the rpc failed.
 *
 * Side effects:
 *    If channel != NULL, the vmx will start sending options to that channel.
 *
 *----------------------------------------------------------------------
 */

Bool
GuestApp_GetUnifiedLoopCap(const char *channel) // IN: send options to which channel?
{
   Bool unifiedLoopCap;

   if (channel) {
      unifiedLoopCap = RpcOut_sendOne(NULL, NULL,
                                      "vmx.capability.unified_loop %s",
                                      channel);
   } else {
      unifiedLoopCap = RpcOut_sendOne(NULL, NULL,
                                      "vmx.capability.unified_loop");
   }

   Debug("Unified loop capability is %d for '%s'\n", unifiedLoopCap,

         channel);

   return unifiedLoopCap;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_GetPtrGrabCap --
 *
 *      Check to see if the VMX supports the functionality that informs
 *      us when the mouse pointer is grabbed
 *
 * Results:
 *      TRUE if VMX supports the pointer grab notification capability
 *      FALSE otherwise
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

Bool
GuestApp_GetPtrGrabCap(const char *channel) // IN
{
   ASSERT(channel);
   return RpcOut_sendOne(NULL, NULL,
                        "vmx.capability.ptr_grab_notification %s",
                        channel);
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_SetVersion --
 *
 *      Set the tools version through the backdoor.
 *
 * Results:
 *      TRUE if the rpci send succeeded
 *      FALSE if it failed
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

Bool
GuestApp_SetVersion(void)
{
   Debug("Setting tools version to '%u'\n", TOOLS_VERSION_CURRENT);

   return RpcOut_sendOne(NULL, NULL, "tools.set.version %u",
                         TOOLS_VERSION_CURRENT);
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_GetCurrentVersion --
 *
 *      Get the current version of this guest app. This wrapper
 *      method is provided so that it can be exported in a COM, etc.
 *      wrapper.
 *
 * Results:
 *      The version.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

uint32
GuestApp_GetCurrentVersion(void)
{
   return TOOLS_VERSION_CURRENT;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_LoadDict --
 *
 *      Load the dict file into memory. Assumes the
 *      given dict has been constructed.
 *
 * Results:
 *      TRUE/FALSE: success/failure
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
GuestApp_LoadDict(GuestApp_Dict *dict)  // IN/OUT
{
   FILE *stream;
   Bool retVal = TRUE;

   ASSERT(dict);
   ASSERT(dict->fileName);

   stream = fopen(dict->fileName, "r");
   if (stream == NULL) {
      Debug("Unable to open \"%s\"\n", dict->fileName);

      return FALSE;
   }

   for (;;) {
      char *name;
      char *value;
      char *line;
      int status;

      status = DictLL_ReadLine(stream, &line, &name, &value);
      if (status == 0) {
         Warning("Unable to read a line from \"%s\": %s\n", dict->fileName,
                 Msg_ErrString());

         retVal = FALSE;
         goto close;
      } else if (status == 1) {
         retVal = TRUE;
         goto close;
      } else if (status != 2) {
         NOT_IMPLEMENTED();
      }

      if (name) {
         ASSERT(value);

         /*
          * We've gotten a valid name/value pair so store it.
          */
         GuestApp_SetDictEntry(dict, name, value);

         free(name);
         free(value);
         free(line);
      }
   }

 close:
   if (fclose(stream)) {
      Warning("Unable to close \"%s\": %s\n", dict->fileName, Msg_ErrString());

      return FALSE;
   }

   if (retVal) {
      dict->fileModTime = File_GetModTime(dict->fileName);
      Debug("Loaded dict from '%s' with mod time=%"FMT64"d\n",
            dict->fileName, dict->fileModTime);
   }

   return retVal;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_WriteDict --
 *
 *      Write the dict file onto disk. Assumes the
 *      given dict has been constructed.
 *
 * Results:
 *      TRUE/FALSE: success/failure
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
GuestApp_WriteDict(GuestApp_Dict *dict)  // IN/OUT
{
   FILE *stream;
   Bool retVal = TRUE;
   GuestApp_DictEntry *p;

   ASSERT(dict);
   ASSERT(dict->fileName);

   stream = fopen(dict->fileName, "w");
   if (stream == NULL) {
      Warning("Unable to open \"%s\"\n", dict->fileName);

      return FALSE;
   }

   for (p = dict->head.next; p; p = p->next) {
      if (!DictLL_WriteLine(stream, p->name, p->value)) {
         Warning("Unable to write line to \"%s\": %s\n", dict->fileName,
                 Msg_ErrString());

         retVal = FALSE;
         break;
      }
   }

   if (fclose(stream)) {
      Warning("Unable to close \"%s\": %s\n", dict->fileName, Msg_ErrString());

      return FALSE;
   }

   if (retVal) {
      dict->fileModTime = File_GetModTime(dict->fileName);
      Debug("Wrote dict to '%s' with mod time=%"FMT64"d\n", dict->fileName,
            dict->fileModTime);
   }

   return retVal;
}

/*
 *----------------------------------------------------------------------
 *
 * GuestApp_GetInstallPath --
 *
 *      Get the tools installation path.
 *
 * Results:
 *      The path.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

const char *
GuestApp_GetInstallPath(void)
{
#if defined(N_PLAT_NLM)
   return "SYS:\\ETC\\VMWTOOL";
#elif defined(_WIN32)
   LONG rv;
   HKEY key;
   DWORD type;
   DWORD len = MAX_PATH;
   static char path[MAX_PATH] = "";

   if (strcmp(path, "") == 0) {
      const char *keyName = "Software\\VMware, Inc.\\VMware Tools";

      rv = RegOpenKey(HKEY_LOCAL_MACHINE, keyName, &key);

      if (rv != ERROR_SUCCESS) {
         Warning("Unable to open key '%s': %s\n", keyName, Msg_ErrString());
         return NULL;
      }
      rv = RegQueryValueEx(key, "InstallPath", 0, &type, (LPBYTE) path, &len);

      RegCloseKey(key);
      if (rv != ERROR_SUCCESS) {
         Warning("Unable to retrieve key '%s': %s\n", keyName, Msg_ErrString());
         goto error;
      }
   }

   /*
    * Strip off the trailing backslash.  This needs to be done with wchars to
    * ensure that we don't mess up a path that ends with the 5C character.
    */
   {
      WCHAR pathWide[MAX_PATH];
      size_t posLastChar;

      if (!MultiByteToWideChar(CP_ACP, 0, path, -1, pathWide, sizeof pathWide)) {
         ASSERT(FALSE);
         goto error;
      }

      posLastChar = wcslen(pathWide) - 1;
      if (pathWide[posLastChar] == L'\\') {
         pathWide[posLastChar] = L'\0';
      }

      if (!WideCharToMultiByte(CP_ACP, 0, pathWide, -1,
                               path, sizeof path, NULL, NULL)) {
         ASSERT(FALSE);
         goto error;
      }
      return path;
   }

 error:
   Str_Strcpy(path, "", sizeof path);
   return NULL;
#else
   return "/etc/vmware-tools";
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_GetConfPath --
 *
 *      Get the path to the Tools configuration file.
 *
 *      XXX: Unfortunately, much of this function is duplicated in
 *      lib/user/win32util.c because we can't use that file inside guest
 *      code. If we do, we'll break Win95 Tools.
 *
 *      XXX: For Windows, this function will only fail on pre-2k/Me systems 
 *      that haven't installed IE4 _and_ haven't ever used our installer 
 *      (it is thought that the installer will copy shfolder.dll if the 
 *      system needs it).
 *
 *      However, the function will also return NULL if we fail to create
 *      a "VMware/VMware Tools" directory. This can occur if we're not running
 *      as Administrator, which VMwareUser doesn't. But I believe that
 *      VMwareService will always come up before VMwareUser, so by the time
 *      a non-root user process calls this function, the directory exists.
 *
 * Results:
 *      The path, or NULL on failure. 
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

const char *
GuestApp_GetConfPath(void)
{
#if defined(_WIN32)
   static char path[MAX_PATH] = "";

   /*
    * XXX: This is racy. But GuestApp_GetInstallPath is racy too. Clearly
    * that is a good enough justification.
    */
   if (strcmp(path, "") == 0) {

      if (!pfnSHGetFolderPath) {
         HMODULE h = LoadLibrary("shfolder.dll");
         if (h) {
            pfnSHGetFolderPath = (PSHGETFOLDERPATH) 
               GetProcAddress(h, "SHGetFolderPathA");
         }
         
         /* win32util.c avoids calling FreeLibrary() so we will too. */
      }

      /* 
       * Get the Common Application data folder - create if it doesn't 
       * exist.
       */
      if (!pfnSHGetFolderPath ||
          FAILED(pfnSHGetFolderPath(NULL, CSIDL_COMMON_APPDATA | 
                                    CSIDL_FLAG_CREATE, NULL, 0, path))) {
         return NULL;
      }
   
      ASSERT(path[0]);
   
      /* Check to see if <product> subdirectories exist. */
      Str_Strcat(path, "\\" PRODUCT_GENERIC_NAME, MAX_PATH);
      if (!File_Exists(path)) {
         if (!CreateDirectory(path, NULL)) {
            return NULL;
         }
      }
      
      if (!File_IsDirectory(path)) {
         return NULL;
      }
      
      Str_Strcat(path, "\\", MAX_PATH);
      Str_Strcat(path, ProductState_GetName(), MAX_PATH);
      if (!File_Exists(path)) {
         if (!CreateDirectory(path, NULL)) {
            return NULL;
         }
      }
      
      if (!File_IsDirectory(path)) {
         return NULL;
      }
   }
   return path;
#else
   /* Just call into GuestApp_GetInstallPath. */
   return GuestApp_GetInstallPath();
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_GetLogPath --
 *
 *      Get the path that the Tools should log to.
 *
 * Results:
 *      Allocates the path or NULL on failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
GuestApp_GetLogPath(void)
{
#if defined(_WIN32)
   /* We should log to %TEMP%. */
   LPTSTR buffer = NULL;
   DWORD bufferSize = 0, neededSize;
   
   if ((neededSize = GetEnvironmentVariable("TEMP", buffer, bufferSize)) == 0) {
      return NULL;
   }
   buffer = malloc(neededSize);
   if (buffer == NULL) {
      return NULL;
   }
   bufferSize = neededSize;
   if (GetEnvironmentVariable("TEMP", buffer, bufferSize) != neededSize) {
      free(buffer);
      return NULL;
   }
   return buffer;
#else
   /* XXX: Is this safe for EVERYONE who isn't Windows? */
   return strdup("/var/log");
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_GetCmdOutput --
 *
 *      Run a cmd & get its cmd line output
 *
 * Results:
 *      An allocated string or NULL if an error occurred.
 *
 * Side effects:
 *	The cmd is run.
 *
 *----------------------------------------------------------------------
 */

char *
GuestApp_GetCmdOutput(const char *cmd) // IN
{
#ifdef N_PLAT_NLM
   Debug("Trying to execute command \"%s\" and catch its output... No way on NetWare...\n", cmd);
   return NULL;
#else
   DynBuf db;
   FILE *stream;
   char *out = NULL;

   DynBuf_Init(&db);

#ifndef _WIN32
   stream = popen(cmd, "r");
#else
   stream = _popen(cmd, "r");
#endif

   if (stream == NULL) {
      Debug("Unable to get output of command \"%s\"\n", cmd);

      return NULL;
   }

   for (;;) {
      char *line = NULL;
      size_t size;

      switch (StdIO_ReadNextLine(stream, &line, 0, &size)) {
      case StdIO_Error:
         goto close;
         break;

      case StdIO_EOF:
         break;

      case StdIO_Success:
         break;

      default:
         ASSERT_NOT_IMPLEMENTED(FALSE);
      }

      if (line == NULL) {
         break;
      }

      DynBuf_Append(&db, line, size);
      free(line);
   }

   if (DynBuf_Get(&db)) {
      out = (char *)DynBuf_AllocGet(&db);
   }
 close:
   DynBuf_Destroy(&db);

#ifndef _WIN32
   pclose(stream);
#else
   _pclose(stream);
#endif

   return out;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_IsHgfsCapable --
 *
 *      Is the host capable of handling hgfs requests?
 *
 * Results:
 *      TRUE if hgfs capable.
 *      FALSE if not hgfs capable
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
GuestApp_IsHgfsCapable()
{
   return RpcOut_sendOne(NULL, NULL, HGFS_SYNC_REQREP_CLIENT_CMD);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_GetPos --
 *
 *      Retrieve the host notion of the guest pointer location. --hpreg
 *
 * Results:
 *      '*x' and '*y' are the coordinates (top left corner is 0, 0) of the
 *      host notion of the guest pointer location. (-100, -100) means that the
 *      mouse is not grabbed on the host.
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

void
GuestApp_GetPos(int16 *x, // OUT
                int16 *y) // OUT
{
   Backdoor_proto bp;

   if (runningInForeignVM) {
      *x = 0;
      *y = 0;
   } else {
      bp.in.cx.halfs.low = BDOOR_CMD_GETPTRLOCATION;
      Backdoor(&bp);
      *x = bp.out.ax.word >> 16;
      *y = bp.out.ax.word;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_SetPos --
 *
 *      Update the host notion of the guest pointer location. 'x' and 'y' are
 *      the coordinates (top left corner is 0, 0). --hpreg
 *
 * Results:
 *      None
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

void
GuestApp_SetPos(uint16 x, // IN
                uint16 y) // IN
{
   Backdoor_proto bp;

   if (!runningInForeignVM) {
      bp.in.cx.halfs.low = BDOOR_CMD_SETPTRLOCATION;
      bp.in.size = (x << 16) | y;
      Backdoor(&bp);
   }
}


/*
 * XXX The 5 functions below should be re-implemented using the message layer,
 *     to benefit from:
 *     . The high-bandwidth backdoor or the generic "send 4 bytes at a time"
 *       logic of the low-bandwidth backdoor
 *     . The restore/resume detection logic
 *     --hpreg
 */

/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_GetHostSelectionLen --
 *
 *      Retrieve the length of the clipboard (if any) to receive from the
 *      VMX. --hpreg
 *
 * Results:
 *      Length >= 0 if a clipboard must be retrieved from the host.
 *      < 0 on error (VMWARE_DONT_EXCHANGE_SELECTIONS or
 *                    VMWARE_SELECTION_NOT_READY currently)
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

int32
GuestApp_GetHostSelectionLen(void)
{
   Backdoor_proto bp;

   if (runningInForeignVM) {
      return(0);
   } else {
      bp.in.cx.halfs.low = BDOOR_CMD_GETSELLENGTH;
      Backdoor(&bp);
      return bp.out.ax.word;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestAppGetNextPiece --
 *
 *      Retrieve the next 4 bytes of the host clipboard. --hpreg
 *
 * Results:
 *      The next 4 bytes of the host clipboard.
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

static uint32
GuestAppGetNextPiece(void)
{
   Backdoor_proto bp;

   if (runningInForeignVM) {
      return(0); 
   } else {
      bp.in.cx.halfs.low = BDOOR_CMD_GETNEXTPIECE;
      Backdoor(&bp);
      return bp.out.ax.word;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_GetHostSelection --
 *
 *      Retrieve the host clipboard. 'data' must be a buffer whose size is at
 *      least (('size' + 4 - 1) / 4) * 4 bytes. --hpreg
 *
 * Results:
 *      The host clipboard in 'data'.
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

void
GuestApp_GetHostSelection(unsigned int size, // IN
                          char *data)        // OUT
{
   uint32 *current;
   uint32 const *end;

   current = (uint32 *)data;
   end = current + (size + sizeof *current - 1) / sizeof *current;
   for (; current < end; current++) {
      *current = GuestAppGetNextPiece();
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_SetSelLength --
 *
 *      Tell the VMX about the length of the clipboard we are about to send
 *      to it. --hpreg
 *
 * Results:
 *      None
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

void
GuestApp_SetSelLength(uint32 length) // IN
{
   Backdoor_proto bp;

   if (!runningInForeignVM) {
      bp.in.cx.halfs.low = BDOOR_CMD_SETSELLENGTH;
      bp.in.size = length;
      Backdoor(&bp);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_SetNextPiece --
 *
 *      Send the next 4 bytes of the guest clipboard. --hpreg
 *
 * Results:
 *      None
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

void
GuestApp_SetNextPiece(uint32 data) // IN
{
   Backdoor_proto bp;

   if (!runningInForeignVM) {
      bp.in.cx.halfs.low = BDOOR_CMD_SETNEXTPIECE;
      bp.in.size = data;
      Backdoor(&bp);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_SetDeviceState --
 *
 *      Ask the VMX to change the connected state of a device. --hpreg
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
GuestApp_SetDeviceState(uint16 id,      // IN: Device ID
                        Bool connected) // IN
{
   Backdoor_proto bp;

   if (runningInForeignVM) {
      return(TRUE);
   } else {
      bp.in.cx.halfs.low = BDOOR_CMD_TOGGLEDEVICE;
      bp.in.size = (connected ? 0x80000000 : 0) | id;
      Backdoor(&bp);
      return bp.out.ax.word ? TRUE : FALSE;
   }
}


/*
 * XXX The 2 functions below should be re-implemented using the message layer,
 *     to benefit from the high-bandwidth backdoor or the generic "send 4
 *     bytes at a time" logic. --hpreg
 */

/*
 *-----------------------------------------------------------------------------
 *
 * GuestAppGetDeviceListElement --
 *
 *      Retrieve 4 bytes of of information about a removable device. --hpreg
 *
 * Results:
 *      TRUE on success. '*data' is set
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestAppGetDeviceListElement(uint16 id,     // IN : Device ID
                             uint16 offset, // IN : Offset in the RD_Info
                                            //      structure
                             uint32 *data)  // OUT: Piece of RD_Info structure
{
   Backdoor_proto bp;

   if (runningInForeignVM) {
      *data = 0;
      return TRUE;
   } else {
      bp.in.cx.halfs.low = BDOOR_CMD_GETDEVICELISTELEMENT;
      bp.in.size = (id << 16) | offset;
      Backdoor(&bp);
      if (bp.out.ax.word == FALSE) {
         return FALSE;
      }
      *data = bp.out.bx.word;
      return TRUE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_GetDeviceInfo --
 *
 *      Retrieve information about a removable device. --hpreg
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
GuestApp_GetDeviceInfo(uint16 id,     // IN: Device ID
                       RD_Info *info) // OUT
{
   uint16 offset;
   uint32 *p;

   /*
    * XXX It is theoretically possible to SEGV here as we can write up to 3
    *     bytes beyond the end of the 'info' structure. I think alignment
    *     saves us in practice. --hpreg
    */
   for (offset = 0, p = (uint32 *)info;
        offset < sizeof *info;
        offset += sizeof (uint32), p++) {
      if (GuestAppGetDeviceListElement(id, offset, p) == FALSE) {
         return FALSE;
      }
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_HostCopyStep --
 *
 *      Do the next step of a host copy operation. --hpreg
 *
 *      This backdoor function allows the guest to ask the VMX to send it the
 *      content of arbitrary files. That is why it is only available in devel
 *      builds in the VMX.
 *
 *      In Windows guests, it is already deprecated by the DnD feature.
 *      In Linux/FreeBSD/NetWare guests, it will soon be deprecated either by
 *      the DnD feature, or by some kind of "Copy host file to guest" GUI menu
 *      entry (effectively a poor man's host to guest DnD). All those solutions
 *      are based on letting the host fully in control, and simply running an
 *      HGFS server in the guest.
 *
 * Results:
 *      The result of the step
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

uint32
GuestApp_HostCopyStep(uint8 c) // IN
{
   Backdoor_proto bp;

   if (runningInForeignVM) {
      return(0);
   } else {
      bp.in.cx.halfs.low = BDOOR_CMD_HOSTCOPY;
      bp.in.size = c;
      Backdoor(&bp);
      return bp.out.ax.word;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_RpcSendOneUtf8 --
 *
 *      Wrapper for RpcOut_SendOneRaw. Sends a single RPCI command with a single string
 *      argument. The argument will be UTF-8 encoded and sent safely over the backdoor.
 *      This used instead of RpcOut_SendOne because that function uses Str_Asprintf
 *      which uses FormatMessage. FormatMessage would interpret the utf8 encoded arg
 *      as being in the current locale and corrupts the string. This is used for
 *      commands which pass file paths over the backdoor.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
GuestApp_RpcSendOneUtf8(char const *cmd,  // IN: RPCI command
                        char const *arg,  // IN: string arg to be Utf8 encoded
                        size_t argSize)   // IN: size of arg
{
   char *utf8Name;
   size_t utf8NameSize;
   char *rpcMessage;
   size_t messageSize;

   ASSERT(cmd);
   ASSERT(arg);

   if (!CodeSet_CurrentToUtf8(arg, argSize,
                               &utf8Name, &utf8NameSize)) {
      Debug("GuestApp_RpcSendOneUtf8: error failed to convert arg to utf8\n");
      return FALSE;
   }

   messageSize = strlen(cmd) + 1 + utf8NameSize + 1;
   rpcMessage = (char *)malloc(messageSize);
   if (!rpcMessage) {
      Debug("GuestApp_RpcSendOneUtf8: Error, out of memory\n");
      free(utf8Name);
      return FALSE;
   }

   Str_Strcpy(rpcMessage, cmd, messageSize);
   Str_Strcat(rpcMessage, " ", messageSize);
   Str_Strcat(rpcMessage, utf8Name, messageSize);
   free(utf8Name);

   Debug("GuestApp_RpcSendOneUtf8: about to send rpc message = *%s*, len = %"FMTSZ"d\n",
         rpcMessage, messageSize);
   if (!RpcOut_SendOneRaw(rpcMessage, messageSize, NULL, NULL)) {
      Debug("GuestApp_RpcSendOneUtf8: Failed to send message to host\n");
      free(rpcMessage);
      return FALSE;
   } else {
      Debug("GuestApp_RpcSendOneUtf8: sent rpc message");
   }

   free(rpcMessage);
   return TRUE;
}


#if !defined(N_PLAT_NLM)
/*
 *----------------------------------------------------------------------------
 *
 * GuestApp_RpcSendOneUtf8CPName --
 *
 *    Wrapper for GuestApp_RpcSendOneCPName to enable both UTF-8 and CPName
 *    conversions.  Sends a single RPCI command with arg UTF-8 encoded and
 *    cpNameArg both UTF-8 and CPName encoded.
 *
 *    The UTF-8 encoded string is optional so that one can send a single UTF-8
 *    and CPName encoded string using this function.
 *
 *    Note that the UTF-8 string always preceeds the UTF-8 & CPName string in
 *    the resulting message.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

Bool
GuestApp_RpcSendOneUtf8CPName(char const *cmd,        // IN: RPCI command
                              char const *arg,        // IN: to UTF8 encode
                              size_t argSize,         // IN: size of arg
                              char delimiter,         // IN: arg/cpNameArg delim.
                              char const *cpNameArg,  // IN: to UTF8/CPName encode
                              size_t cpNameArgSize)   // IN: size of cpNameArg
{
   size_t utf8cpNameArgSize;
   char *utf8cpNameArg;
   Bool ret = FALSE;

   ASSERT(cmd);
   ASSERT(cpNameArg);


   /* UTF-8 encode cpNameArg */
   Debug("GuestApp_RpcSendOneUtf8CPName: cpNameArg=\"%s\" (%"FMTSZ"u)\n",
         CPName_Print(cpNameArg, cpNameArgSize), cpNameArgSize);

   if (!CodeSet_CurrentToUtf8(cpNameArg, cpNameArgSize,
                              &utf8cpNameArg, &utf8cpNameArgSize)) {
      Debug("GuestApp_RpcSendOneUtf8CPName: error failed to convert arg to utf8\n");
      return FALSE;
   }

   Debug("GuestApp_RpcSendOneUtf8CPName: utf8cpNameArg=\"%s\", len=%"FMTSZ"d\n",
         CPName_Print(utf8cpNameArg, utf8cpNameArgSize), utf8cpNameArgSize);

   /* UTF-8 encode arg if provided */
   if (arg) {
      char *rpcMessage;
      size_t messageSize;
      char *utf8Arg;
      size_t utf8ArgSize = 0;

      Debug("GuestApp_RpcSendOneUtf8CPName: arg=\"%s\"\n", arg);

      if (!CodeSet_CurrentToUtf8(arg, argSize, &utf8Arg, &utf8ArgSize)) {
         Debug("GuestApp_RpcSendOneUtf8CPName: error failed to convert arg to utf8\n");
         goto abort;
      }

      Debug("GuestApp_RpcSendOneUtf8CPName: utf8Arg=\"%s\", len=%"FMTSZ"d\n",
            utf8Arg, utf8ArgSize);

      /* Merge command and UTF-8 encoded string */
      messageSize = strlen(cmd) + 1 + utf8ArgSize + 1;
      rpcMessage = (char *)malloc(messageSize);
      if (!rpcMessage) {
         Debug("GuestApp_RpcSendOneUtf8CPName: Error, out of memory\n");
         free(utf8Arg);
         goto abort;
      }

      Str_Strcpy(rpcMessage, cmd, messageSize);
      Str_Strcat(rpcMessage, " ", messageSize);
      Str_Strcat(rpcMessage, utf8Arg, messageSize);
      free(utf8Arg);

      ret = GuestApp_RpcSendOneCPName(rpcMessage, delimiter,
                                      utf8cpNameArg, utf8cpNameArgSize);
      free(rpcMessage);
   } else {
      ret = GuestApp_RpcSendOneCPName(cmd, delimiter,
                                      utf8cpNameArg, utf8cpNameArgSize);
   }

abort:
   free(utf8cpNameArg);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_RpcSendOneCPName --
 *
 *    Wrapper for RpcOut_SendOneRaw. Sends a single RPCI command with
 *    a single string argument. The argument will be CPName encoded and sent
 *    safely over the backdoor.
 *
 *    This is used instead of RpcOut_sendOne because that function uses
 *    Str_Asprintf, which uses FormatMessage.  FormatMessage would interpret
 *    the encoded arg as being in the current locale and corrupts the string
 *    if it is also UTF-8 encoded.  The string will certainly be UTF-8
 *    encoded when this function is invoked by the
 *    GuestApp_RpcSendOneUtf8CPName() wrapper.  This function is used for
 *    commands which pass file paths over the backdoor.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
GuestApp_RpcSendOneCPName(char const *cmd,  // IN: RPCI command
                          char delimiter,   // IN: cmd and arg delimiter
                          char const *arg,  // IN: string to CPName encode
                          size_t argSize)   // IN: size of arg
{
   char cpName[HGFS_PACKET_MAX];
   int32 cpNameSize;
   char *rpcMessage;
   size_t cmdSize;
   size_t messageSize;

   ASSERT(cmd);
   ASSERT(arg);

   /*
    * In addition to converting to CPName, this will also prepend  "root" and
    * "drive"|"unc" if needed (for Windows).
    */
   cpNameSize = CPNameUtil_ConvertToRoot(arg,
                                         sizeof cpName,
                                         cpName);
   if (cpNameSize < 0) {
      Debug("GuestApp_RpcSendOneCPName: Error, could not convert to CPName.\n");
      return FALSE;
   }

   cmdSize = strlen(cmd);
   messageSize = cmdSize + 1 + cpNameSize + 1;
   rpcMessage = (char *)malloc(messageSize);
   if (!rpcMessage) {
      Debug("GuestApp_RpcSendOneCPName: Error, out of memory\n");
      return FALSE;
   }

   Debug("GuestApp_RpcSendOneCPName: cpname=\"%s\", len=%d\n",
         CPName_Print(cpName, cpNameSize), cpNameSize);

   memcpy(rpcMessage, cmd, cmdSize);
   rpcMessage[cmdSize] = delimiter;
   memcpy(&rpcMessage[cmdSize + 1], cpName, cpNameSize + 1);

   Debug("GuestApp_RpcSendOneCPName: about to send rpc message = *%s*, len = %"
         FMTSZ"u\n", CPName_Print(rpcMessage, messageSize), messageSize);

   if (!RpcOut_SendOneRaw(rpcMessage, messageSize, NULL, NULL)) {
      Debug("GuestApp_RpcSendOneCPName: Failed to send message to host\n");
      free(rpcMessage);
      return FALSE;
   }

   free(rpcMessage);
   return TRUE;
}
#endif

#ifdef __cplusplus
}
#endif
