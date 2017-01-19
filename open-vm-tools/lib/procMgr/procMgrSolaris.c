/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * procMgrSolaris.c --
 *
 *    Solaris specific implementations of the process management lib methods.
 *
 */


#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#if _FILE_OFFSET_BITS != 64
#   error FILE_OFFSET_BITS=64 required
#endif
/*
 * Work around unnecessary blocking of FOB=64 in Sol10, fixed in Sol11
 * See https://www.illumos.org/issues/2410
 */
#undef _FILE_OFFSET_BITS
#include <procfs.h>
#define _FILE_OFFSET_BITS 64

#include <ctype.h>
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>

#include "vmware.h"
#include "procMgr.h"
#include "debug.h"
#include "util.h"
#undef offsetof
#include "dynbuf.h"
#include "dynarray.h"
#include "su.h"
#include "str.h"
#include "posix.h"
#include "codeset.h"
#include "unicode.h"

static Bool
ReadArgsFromAddressSpaceFile(int asFd,
                             psinfo_t *psInfo,
                             DynBufArray *cmdLineArr);

static Bool
ReadOffsetsFromAddressSpaceFile(int asFd,
                                psinfo_t *psInfo,
                                uintptr_t *argOffs);

static char *
ExtractArgStringFromAddressSpaceFile(int asFd,
                                     uintptr_t offset);

static char *
ExtractCommandLineFromAddressSpaceFile(psinfo_t *procInfo,
                                       char **procCmdName);

/*
 *----------------------------------------------------------------------------
 *
 * ProcMgr_ListProcesses --
 *
 *      List all the processes that the calling client has privilege to
 *      enumerate. The strings in the returned structure should be all
 *      UTF-8 encoded, although we do not enforce it right now.
 *
 * Results:
 *
 *      On success, the process list is returned and the caller is responsible
 *      for freeing the memory used by calling ProcMgr_FreeProcList. On
 *      failure, NULL is returned.
 *
 * Side effects:
 *
 *      None
 *
 *----------------------------------------------------------------------------
 */

ProcMgrProcInfoArray *
ProcMgr_ListProcesses(void)
{
   ProcMgrProcInfoArray *procList = NULL;
   ProcMgrProcInfo processInfo;
   Bool failed = TRUE;
   DIR *dir;
   struct dirent *ent;

   procList = Util_SafeCalloc(1, sizeof *procList);
   ProcMgrProcInfoArray_Init(procList, 0);
   processInfo.procOwner = NULL;
   processInfo.procCmdLine = NULL;
   processInfo.procCmdName = NULL;

   dir = opendir("/proc");
   if (NULL == dir) {
      Warning("ProcMgr_ListProcesses unable to open /proc\n");
      goto exit;
   }

   while (TRUE) {
      char *tmp;
      char *cmdNameBegin = NULL;
      char *cmdNameEnd = NULL;
      struct passwd *pwd;
      char tempPath[MAXPATHLEN];
      psinfo_t procInfo;
      size_t strLen = 0;
      size_t numRead = 0;
      int psInfoFd = -1;
      Bool cmdNameLookup = TRUE;

      errno = 0;

      ent = readdir(dir);
      if (ent == NULL) {
         if (errno == 0) {
            break;
         } else {
            goto exit;
         }
      }

      if (Str_Snprintf(tempPath,
                       sizeof tempPath,
                       "/proc/%s/psinfo",
                       ent->d_name) == -1) {
         Debug("Process id '%s' too large\n", ent->d_name);
         continue;
      }
      psInfoFd = Posix_Open(tempPath, O_RDONLY);
      if (psInfoFd == -1) {
         if (errno == ENOENT || errno == EACCES) {
            continue;
         } else {
            goto exit;
         }
      }

      numRead = read(psInfoFd, &procInfo, sizeof procInfo);
      close(psInfoFd);
      if (numRead != sizeof procInfo) {
         goto exit;
      }

      processInfo.procStartTime = procInfo.pr_start.tv_sec;

      /*
       * If the command name in the ps info struct is strictly less than the
       * maximum allowed size, then we can save it right now. Else we shall
       * need to try and parse it from the entire command line, to avoid
       * saving a truncated command name.
       */
      if (strlen(procInfo.pr_fname) + 1 < sizeof procInfo.pr_fname) {
         processInfo.procCmdName = Unicode_Alloc(procInfo.pr_fname,
                                                 STRING_ENCODING_DEFAULT);
         cmdNameLookup = FALSE;
      }

      /*
       * The logic below is this:
       * 1. If we are looking for the explicit command name, we need to
       *    extract the arguments from the /proc/<pid>/as file and save argv[0].
       * 2. If we are not looking for the explicit command name, but the command
       *    line in the ps info struct is not strictly less than the maximum
       *    allowed size, we still need to extract the arguments from the
       *    /proc/<pid>/as file, to avoid saving truncated comand line.
       * 3. Else we can save the command line directly from the ps info struct.
       */
      if (cmdNameLookup) {
         tmp = ExtractCommandLineFromAddressSpaceFile(&procInfo, &processInfo.procCmdName);
      } else if (strlen(procInfo.pr_psargs) + 1 >= sizeof procInfo.pr_psargs) {
         tmp = ExtractCommandLineFromAddressSpaceFile(&procInfo, NULL);
      } else {
         tmp = NULL;
      }

      if (tmp != NULL) {
         processInfo.procCmdLine = Unicode_Alloc(tmp, STRING_ENCODING_DEFAULT);
         cmdNameLookup = FALSE;
      } else {
         /*
          * We had some issues reading procfs, mostly due to lack of
          * permissions for certain system owned precesses. So let's resort to
          * what the procinfo structure provides as a last resort.
          */
         processInfo.procCmdLine = Unicode_Alloc(procInfo.pr_psargs,
                                                 STRING_ENCODING_DEFAULT);

         if (cmdNameLookup) {
            /*
             * Now let's try and get the best effort command name from the entire
             * command line. The method below does not take care of spaces in folder
             * names and executable file names. This is the best we can do at this
             * point, considering that spaces are not common in either file or
             * folder names in Solaris, specially when owned by the system.
             */
            char *tmp2 = Unicode_Alloc(procInfo.pr_psargs, STRING_ENCODING_DEFAULT);
            cmdNameBegin = tmp2;
            /*
             * Assuming the command name to end at the first blank space.
             */
            cmdNameEnd = cmdNameBegin;
            while ('\0' != *cmdNameEnd) {
               if ('/' == *cmdNameEnd) {
                  cmdNameBegin = cmdNameEnd + 1;
               }
               if (' ' == *cmdNameEnd) {
                  break;
               }
               cmdNameEnd++;
            }
            *cmdNameEnd = '\0';
            processInfo.procCmdName = Str_SafeAsprintf(NULL, "%s", cmdNameBegin);
            free(tmp2);
         }
      }
      free(tmp);
      tmp = NULL;

      /*
       * Store the pid.
       */
      processInfo.procId = procInfo.pr_pid;

      /*
       * Store the owner of the process.
       */
      pwd = getpwuid(procInfo.pr_uid);
      processInfo.procOwner = (NULL == pwd)
                              ? Str_SafeAsprintf(&strLen, "%d", (int) procInfo.pr_uid)
                              : Unicode_Alloc(pwd->pw_name, STRING_ENCODING_DEFAULT);

      /*
       * Store the process info into a list buffer.
       */
      if (!ProcMgrProcInfoArray_Push(procList, processInfo)) {
         Warning("%s: failed to expand DynArray - out of memory\n",
                 __FUNCTION__);
         goto exit;
      }
      processInfo.procCmdName = NULL;
      processInfo.procCmdLine = NULL;
      processInfo.procOwner = NULL;
   } // while (TRUE)

   if (0 < ProcMgrProcInfoArray_Count(procList)) {
      failed = FALSE;
   }

exit:
   closedir(dir);

   free(processInfo.procOwner);
   free(processInfo.procCmdLine);
   free(processInfo.procCmdName);

   if (failed) {
      ProcMgr_FreeProcList(procList);
      procList = NULL;
   }

   return procList;
}


/*
 *----------------------------------------------------------------------------
 *
 * ExtractCommandLineFromAddressSpaceFile --
 *
 *      Read the address space file (/proc/<pid>/as) for a given process and
 *      return its command line string.
 *
 * Results:
 *
 *      On success, the command line string for the process is returned and the
 *      caller is responsible for freeing the memory used by this string. On
 *      failure, NULL is returned.
 *
 * Side effects:
 *
 *      None
 *
 *----------------------------------------------------------------------------
 */

static char *
ExtractCommandLineFromAddressSpaceFile(psinfo_t *procInfo, //IN: psinfo struct
                                       char **procCmdName) //OUT: command name
{
   int argc;
   int i;
   char tempPath[MAXPATHLEN];
   char *buf;
   int asFd = -1;
   DynBuf cmdLine;
   DynBufArray args;
   pid_t pid;
   char *cmdNameBegin;

   if (NULL != procCmdName) {
      *procCmdName = NULL;
   }
   pid = procInfo->pr_pid;

   if (Str_Snprintf(tempPath,
                sizeof tempPath,
                "/proc/%"FMT64"d/as",
                (int64_t)pid) == -1) {
      /* This should not happen. MAXPATHLEN should be large enough. */
      ASSERT(FALSE);
   }
   asFd = Posix_Open(tempPath, O_RDONLY);
   if (asFd == -1) {
      Warning("Could not open address space file for pid %"FMT64"d, %s\n",
              (int64_t)pid, strerror(errno));
      return NULL;
   }

   buf = NULL;
   if (ReadArgsFromAddressSpaceFile(asFd, procInfo, &args)) {
      /* Concatenate the strings in args into cmdLine. */
      DynBuf_Init(&cmdLine);
      argc = DynBufArray_Count(&args);
      for (i = 0; i < argc; i++) {
         buf = DynBuf_Get(DynBufArray_AddressOf(&args, i));
         DynBuf_Append(&cmdLine, buf, strlen(buf));
         if (i + 1 < argc) {
            DynBuf_Append(&cmdLine, " ", 1);
         }
         if (NULL != procCmdName && 0 == i) {
            /*
             * Store the command name of the process.
             * Find the last path separator, to get the cmd name.
             * If no separator is found, then use the whole name.
             */
            cmdNameBegin = strrchr(buf, '/');
            if (NULL == cmdNameBegin) {
               cmdNameBegin = buf;
            } else {
               /*
                * Skip over the last separator.
                */
               cmdNameBegin++;
            }
            *procCmdName = Unicode_Alloc(cmdNameBegin, STRING_ENCODING_DEFAULT);
         }
         DynBuf_Destroy(DynBufArray_AddressOf(&args, i));
      }
      DynBuf_AppendString(&cmdLine,"");
      DynBufArray_Destroy(&args);
      DynBuf_Trim(&cmdLine);
      buf = DynBuf_Detach(&cmdLine);
      DynBuf_Destroy(&cmdLine);
   }
   close(asFd);
   return buf;
}


/*
 *----------------------------------------------------------------------------
 *
 * ReadArgsFromAddressSpaceFile --
 *
 *      Read the command line arguments for a process and store them in the
 *      cmdLineArr array. The processes' address space file must be open with
 *      the file descriptor adFd. This function assumes that it runs in the
 *      same locale as the process being inspected.
 *
 * Results:
 *
 *      On success, TRUE is returned and the caller is responsible for
 *      de-allocating the memory used by the DynBufArray; by calling
 *      DynBuf_Destroy on each of its elements, and then DynBufArray_Destroy on
 *      the array itself. FALSE is returned on failure, and no de-allocation
 *      is needed.
 *
 * Side effects:
 *
 *      The cmdLineArr array is filled with the command line strings of the
 *      given process.
 *----------------------------------------------------------------------------
 */

static Bool
ReadArgsFromAddressSpaceFile(int asFd,                  //IN
                             psinfo_t *psInfo,          //IN
                             DynBufArray *cmdLineArr)   //OUT
{
   uintptr_t *argOffs;
   uintptr_t argOff;
   uintptr_t nextArgOff;
   int argc;
   int i;
   char *argBuf;
   char *argBufPtr;
   DynBuf *arg;

   argc = psInfo->pr_argc;
   DynBufArray_Init(cmdLineArr, argc);
   for (i = 0; i < argc; i++) {
      DynBuf_Init(DynBufArray_AddressOf(cmdLineArr, i));
   }
   if (argc == 0) {
      return TRUE;
   }
   argOffs = Util_SafeCalloc(argc, sizeof *argOffs);
   if (argOffs == NULL) {
      return FALSE;
   }

   if (!ReadOffsetsFromAddressSpaceFile(asFd, psInfo, argOffs)) {
      goto fail;
   }

   /* Read the command line arguments into the cmdLineArr array. */
   nextArgOff = argc > 0 ? argOffs[0] : 0;
   i = 0;
   while (i < argc) {
      argOff = argOffs[i];

      /*
       * The argument strings are contiguous in the address space file. So
       * argOff[i] + strlen(arg[i]) + 1 should be equal to argOff[i + 1].
       */
      if ((argOff == 0) || (argOff != nextArgOff)) {
         goto fail;
      }
      argBuf = ExtractArgStringFromAddressSpaceFile(asFd, argOff);
      if (argBuf == NULL) {
         goto fail;
      }
      nextArgOff = argOff + strlen(argBuf) + 1;
      argBufPtr = argBuf +  strlen(argBuf);
      while ((argBufPtr > argBuf) && isspace(*(argBufPtr - 1))) {
         argBufPtr--;
      }
      *argBufPtr = '\0';
      arg = DynBufArray_AddressOf(cmdLineArr, i);
      if (!DynBuf_Append(arg,
                         argBuf,
                         strlen(argBuf) + 1)) {
         free(argBuf);
         goto fail;
      }
      free(argBuf);
      i++;
   }
   return TRUE;

fail:
   Warning("Failed to read command line arguments\n");
   argc = DynBufArray_Count(cmdLineArr);
   for (i = 0; i < argc; i++) {
      arg = DynArray_AddressOf(cmdLineArr, i);
      DynBuf_Destroy(arg);
   }
   DynBufArray_SetCount(cmdLineArr, 0);
   DynBufArray_Destroy(cmdLineArr);
   free(argOffs);
   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * ReadOffsetsFromAddressSpaceFile --
 *
 *      Read the offsets for the command line arguments strings of a process
 *      into the argOffs array. The processes' /proc/<pid>/as file must be
 *      open with the file descriptor asFd. The argOffs array must have enough
 *      space to store all the offsets for the process.
 *
 * Results:
 *
 *      TRUE on success, FALSE on error.
 *
 * Side Effects:
 *
 *      The argOffs array is filled with the offsets of the command line
 *      arguments.
 *
 *----------------------------------------------------------------------------
 */

static Bool
ReadOffsetsFromAddressSpaceFile(int asFd,              //IN
                                psinfo_t *psInfo,      //IN
                                uintptr_t *argOffs)    //OUT
{
   int argc;
   int i;
   uintptr_t argv;
   ssize_t res;

   argc = psInfo->pr_argc;
   argv = psInfo->pr_argv;
   /*
    * The offsets for the command line argument are located at an offset of
    * argv in the /proc/<pid>/as file. If the process data model is NATIVE,
    * each offset is a unitptr_t; else if the data model is ILP32 or LP64, each
    * offset is uint32_t.
    */
   if (psInfo->pr_dmodel == PR_MODEL_NATIVE) {
      /*
       * The offset for each arguments is sizeof uintptr_t bytes.
       */
      res = pread(asFd, argOffs, argc * sizeof argv, argv);
      if (res != argc * sizeof argv) {
         return FALSE;
      }
   } else {
      /*
       * The offset for each arguments is sizeof uint32_t bytes.
       */
      uint32_t *argOffs32;
      argOffs32 = Util_SafeCalloc(argc, sizeof *argOffs32);
      if (argOffs32 == NULL) {
         return FALSE;
      }
      res = pread(asFd, argOffs32, argc * sizeof *argOffs32, argv);
      if (res != argc * sizeof *argOffs32) {
         free (argOffs32);
         return FALSE;
      }
      for (i = 0; i < argc; i++) {
         argOffs[i] = argOffs32[i];
      }
      free(argOffs32);
   }
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * ExtractArgStringFromAddressSpaceFile --
 *
 *      Extract a string at a given offset in the given file. The file must be
 *      open with file descriptor asFd.
 *
 * Results:
 *
 *      On success, the NULL terminated string is returned and the
 *      caller is responsible for freeing the memory used by this string. On
 *      failure, NULL is returned.
 *
 * Side effects:
 *
 *      None
 *
 *----------------------------------------------------------------------------
 */

static char *
ExtractArgStringFromAddressSpaceFile(int asFd,          //IN
                                     uintptr_t offset)  //IN
{
   int readSize = 32;
   char *buf;
   ssize_t res;

   buf = Util_SafeMalloc(readSize * sizeof *buf);
   while (1) {
      res = pread(asFd, buf, readSize, offset);
      if (res != readSize) {
         goto fail;
      }
      if (Str_Strlen(buf, readSize) == readSize) {
         readSize *= 2;
         free(buf);
         if (readSize > NCARGS) {
            return NULL;
         }
         buf = Util_SafeMalloc(readSize * sizeof *buf);
      } else {
         break;
      }
   }
   return buf;

fail:
   free(buf);
   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_ImpersonateUserStart --
 *
 *      Impersonate a user.  Much like bora/lib/impersonate, but
 *      changes the real and saved uid as well, to work with syscalls
 *      (access() and kill()) that look at real UID instead of effective.
 *      The user name should be UTF-8 encoded, although we do not enforce
 *      it right now. Solaris does not have setresuid/setresgid. So perform
 *      a two step process to set the real and effective uid/gid to given
 *      user and leave the saved uid/gid as root.
 *
 *      Assumes it will be called as root.
 *
 * Results:
 *      TRUE on success, FALSE on failure
 *
 * Side effects:
 *
 *      Uid/gid set to given user, saved uid/gid left as root.
 *
 *----------------------------------------------------------------------
 */

Bool
ProcMgr_ImpersonateUserStart(const char *user,  // IN: UTF-8 encoded user name
                             AuthToken token)   // IN
{
   char buffer[BUFSIZ];
   struct passwd pw;
   struct passwd *ppw;
   gid_t root_gid;
   int ret;
   char *userLocal;

   ppw = &pw;
   if ((ppw = getpwuid_r(0, &pw, buffer, sizeof buffer)) == NULL) {
      return FALSE;
   }

   root_gid = ppw->pw_gid;

   /* convert user name to local character set */
   userLocal = (char *)Unicode_GetAllocBytes(user, Unicode_GetCurrentEncoding());
   if (!userLocal) {
       Warning("Failed to convert user name %s to local character set.\n", user);
       return FALSE;
   }

   ppw = getpwnam_r(userLocal, &pw, buffer, sizeof buffer);

   free(userLocal);

   if (ppw == NULL) {
      return FALSE;
   }

   /* first change group. */
   ret = Id_SetGid(root_gid);
   if (ret < 0) {
      Warning("Failed to setregid() for root\n");
      return FALSE;
   }

   /*  Snippet from Solaris setregid man page --
    *
    *  A -1 argument does not change the corresponding gid. If the real user ID
    *  is being changed, or the effective user ID is being changed to a value
    *  not equal to the real user ID, the saved set-user ID is set equal to
    *  the new effective user ID.
    */
   ret = Id_SetREGid(ppw->pw_gid, -1);
   if (ret < 0) {
      Warning("Failed to setregid() for user %s\n", user);
      return FALSE;
   }
   ret = Id_SetREGid(-1, ppw->pw_gid);
   if (ret < 0) {
      Warning("Failed to setregid() for user %s\n", user);
      return FALSE;
   }
   ret = initgroups(ppw->pw_name, ppw->pw_gid);
   if (ret < 0) {
      Warning("Failed to initgroups() for user %s\n", user);
      goto failure;
   }

   /* now user. */
   ret = Id_SetUid(0);
   if (ret < 0) {
      Warning("Failed to setregid() for root\n");
      return FALSE;
   }

   /* Same as above. */
   ret = Id_SetREUid(ppw->pw_uid, -1);
   if (ret < 0) {
      Warning("Failed to setreuid() for user %s\n", user);
      goto failure;
   }
   ret = Id_SetREUid(-1, ppw->pw_uid);
   if (ret < 0) {
      Warning("Failed to setreuid() for user %s\n", user);
      goto failure;
   }

   /* set env. */
   setenv("USER", ppw->pw_name, 1);
   setenv("HOME", ppw->pw_dir, 1);
   setenv("SHELL", ppw->pw_shell, 1);

   return TRUE;

failure:
   /* try to restore on error. */
   ProcMgr_ImpersonateUserStop();
   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_ImpersonateUserStop --
 *
 *      Stop impersonating a user and return to root. Solaris does not
 *      have setresuid/setresgid. So perform a two step process while
 *      restoring uids to root.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *
 *      Uid/gid restored to root.
 *
 *----------------------------------------------------------------------
 */

Bool
ProcMgr_ImpersonateUserStop(void)
{
   char buffer[BUFSIZ];
   struct passwd pw;
   struct passwd *ppw;
   int ret;

   ppw = &pw;
   if ((ppw = getpwuid_r(0, &pw, buffer, sizeof buffer)) == NULL) {
      return FALSE;
   }

   /* first change back user, Do the same two step process as above. */
   ret = Id_SetREUid(-1, ppw->pw_uid);
   if (ret < 0) {
      Warning("Failed setreuid() for root\n");
      return FALSE;
   }
   ret = Id_SetREUid(ppw->pw_uid, -1);
   if (ret < 0) {
      Warning("Failed to setreuid() for root\n");
      return FALSE;
   }

   /* now group. */
   ret = Id_SetGid(ppw->pw_gid);
   if (ret < 0) {
      Warning("Failed to setgid() for root\n");
      return FALSE;
   }
   ret = initgroups(ppw->pw_name, ppw->pw_gid);
   if (ret < 0) {
      Warning("Failed to initgroups() for root\n");
      return FALSE;
   }

   /* set env. */
   setenv("USER", ppw->pw_name, 1);
   setenv("HOME", ppw->pw_dir, 1);
   setenv("SHELL", ppw->pw_shell, 1);

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * ProcMgr_GetImpersonatedUserInfo --
 *
 *      Return info about the impersonated user.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

Bool
ProcMgr_GetImpersonatedUserInfo(char **userName,            // OUT
                                char **homeDir)             // OUT
{
   char buffer[BUFSIZ];
   struct passwd pw;
   struct passwd *ppw;

   *userName = NULL;
   *homeDir = NULL;

   ppw = &pw;
   if ((ppw = getpwuid_r(Id_GetEUid(), &pw, buffer, sizeof buffer)) == NULL) {
      return FALSE;
   }

   *userName = Unicode_Alloc(ppw->pw_name, STRING_ENCODING_DEFAULT);
   *homeDir = Unicode_Alloc(ppw->pw_dir, STRING_ENCODING_DEFAULT);

   return TRUE;
}

