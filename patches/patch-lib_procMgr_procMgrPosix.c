$NetBSD$

--- lib/procMgr/procMgrPosix.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/procMgr/procMgrPosix.c
@@ -27,8 +27,8 @@
 // pull in setresuid()/setresgid() if possible
 #define  _GNU_SOURCE
 #include <unistd.h>
-#if !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
-#include <asm/param.h>
+#if !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__) && !defined(__NetBSD__)
+#include <asm/param.h> 
 #endif
 #if !defined(sun) && !defined(__APPLE__)
 #include <locale.h>
@@ -47,13 +47,13 @@
 #include <time.h>
 #include <grp.h>
 #include <sys/syscall.h>
-#if defined(linux) || defined(__FreeBSD__) || defined(HAVE_SYS_USER_H)
+#if defined(linux) || defined(__FreeBSD__) || defined(HAVE_SYS_USER_H) || defined(__NetBSD__)
 // sys/param.h is required on FreeBSD before sys/user.h
 #   include <sys/param.h>
 // Pull in PAGE_SIZE/PAGE_SHIFT defines ahead of vm_basic_defs.h
 #   include <sys/user.h>
 #endif
-#if defined (__FreeBSD__)
+#if defined(__FreeBSD__) || defined(__NetBSD__)
 #include <kvm.h>
 #include <limits.h>
 #include <paths.h>
@@ -633,7 +633,7 @@ abort:
  *----------------------------------------------------------------------
  */
 
-#if defined(__FreeBSD__)
+#if defined(__FreeBSD__) || defined(__NetBSD__)
 ProcMgrProcInfoArray *
 ProcMgr_ListProcesses(void)
 {
@@ -664,7 +664,8 @@ ProcMgr_ListProcesses(void)
    /*
     * Get the list of process info structs
     */
-   kp = kvm_getprocs(kd, KERN_PROC_PROC, flag, &nentries);
+   //kp = kvm_getprocs(kd, KERN_PROC_PROC, flag, &nentries);
+   kp = kvm_getprocs(kd, KERN_PROC_ALL, flag, &nentries);
    if (kp == NULL || nentries <= 0) {
       Warning("%s: failed to get proc infos with error: %s\n",
               __FUNCTION__, kvm_geterr(kd));
@@ -684,7 +685,7 @@ ProcMgr_ListProcesses(void)
     * Iterate through the list of process entries
     */
    for (i = 0; i < nentries; ++i, ++kp) {
-      struct passwd *pwd;
+      //struct passwd *pwd;
       char **cmdLineTemp = NULL;
       char *cmdNameBegin = NULL;
       Bool cmdNameLookup = TRUE;
@@ -692,23 +693,29 @@ ProcMgr_ListProcesses(void)
       /*
        * Store the pid of the process.
        */
-      procInfo.procId = kp->ki_pid;
+      //procInfo.procId = kp->ki_pid;
+      procInfo.procId = kp->kp_proc.p_pid;
+
 
+      // FIX FOR NETBSD
       /*
        * Store the owner of the process.
        */
-      pwd = getpwuid(kp->ki_uid);
-      procInfo.procOwner = (NULL == pwd)
-                           ? Str_SafeAsprintf(NULL, "%d", (int) kp->ki_uid)
-                           : Unicode_Alloc(pwd->pw_name, STRING_ENCODING_DEFAULT);
+      //pwd = getpwuid(kp->ki_uid);
+      procInfo.procOwner = Str_SafeAsprintf(NULL, "%d", (int) kp->kp_eproc.e_ppid); 
+//(NULL == pwd)
+//                           ? Str_SafeAsprintf(NULL, "%d", (int) kp->ki_uid)
+//                           : Unicode_Alloc(pwd->pw_name, STRING_ENCODING_DEFAULT);
 
       /*
        * If the command name in the kinfo_proc struct is strictly less than the
        * maximum allowed size, then we can save it right now. Else we shall
        * need to try and parse it from the entire command line.
        */
-      if (strlen(kp->ki_comm) + 1 < sizeof kp->ki_comm) {
-         procInfo.procCmdName = Unicode_Alloc(kp->ki_comm, STRING_ENCODING_DEFAULT);
+      //if (strlen(kp->ki_comm) + 1 < sizeof kp->ki_comm) {
+      if (strlen(kp->kp_proc.p_comm) + 1 < sizeof kp->kp_proc.p_comm) {
+         //procInfo.procCmdName = Unicode_Alloc(kp->ki_comm, STRING_ENCODING_DEFAULT);
+         procInfo.procCmdName = Unicode_Alloc(kp->kp_proc.p_comm, STRING_ENCODING_DEFAULT);
          cmdNameLookup = FALSE;
       }
 
@@ -771,9 +778,11 @@ ProcMgr_ListProcesses(void)
          procInfo.procCmdLine = DynBuf_Detach(&dbuf);
          DynBuf_Destroy(&dbuf);
       } else {
-         procInfo.procCmdLine = Unicode_Alloc(kp->ki_comm, STRING_ENCODING_DEFAULT);
+         //procInfo.procCmdLine = Unicode_Alloc(kp->ki_comm, STRING_ENCODING_DEFAULT);
+         procInfo.procCmdLine = Unicode_Alloc(kp->kp_proc.p_comm, STRING_ENCODING_DEFAULT);
          if (cmdNameLookup) {
-            procInfo.procCmdName = Unicode_Alloc(kp->ki_comm, STRING_ENCODING_DEFAULT);
+            //procInfo.procCmdName = Unicode_Alloc(kp->ki_comm, STRING_ENCODING_DEFAULT);
+            procInfo.procCmdName = Unicode_Alloc(kp->kp_proc.p_comm, STRING_ENCODING_DEFAULT);
             cmdNameLookup = FALSE;
          }
       }
@@ -781,7 +790,8 @@ ProcMgr_ListProcesses(void)
       /*
        * Store the start time of the process
        */
-      procInfo.procStartTime = kp->ki_start.tv_sec;
+      // FIX FOR NETBSD
+      procInfo.procStartTime = 0; //kp->ki_start.tv_sec;
 
       /*
        * Store the process info pointer into a list buffer.
@@ -2033,7 +2043,7 @@ ProcMgr_Free(ProcMgr_AsyncProc *asyncPro
    free(asyncProc);
 }
 
-#if defined(linux) || defined(__FreeBSD__) || defined(__APPLE__)
+#if defined(linux) || defined(__FreeBSD__) || defined(__APPLE__) || defined(__NetBSD__)
 
 /*
  *----------------------------------------------------------------------
@@ -2106,7 +2116,7 @@ ProcMgr_ImpersonateUserStart(const char 
    // first change group
 #if defined(USERWORLD)
    ret = Id_SetREGid(ppw->pw_gid, ppw->pw_gid);
-#elif defined(__APPLE__)
+#elif defined(__APPLE__) || defined(__NetBSD__)
    ret = setregid(ppw->pw_gid, ppw->pw_gid);
 #else
    ret = setresgid(ppw->pw_gid, ppw->pw_gid, root_gid);
@@ -2125,7 +2135,7 @@ ProcMgr_ImpersonateUserStart(const char 
    // now user
 #if defined(USERWORLD)
    ret = Id_SetREUid(ppw->pw_uid, ppw->pw_uid);
-#elif defined(__APPLE__)
+#elif defined(__APPLE__) || defined(__NetBSD__)
    ret = setreuid(ppw->pw_uid, ppw->pw_uid);
 #else
    ret = setresuid(ppw->pw_uid, ppw->pw_uid, 0);
@@ -2187,7 +2197,7 @@ ProcMgr_ImpersonateUserStop(void)
    // first change back user
 #if defined(USERWORLD)
    ret = Id_SetREUid(ppw->pw_uid, ppw->pw_uid);
-#elif defined(__APPLE__)
+#elif defined(__APPLE__) || defined(__NetBSD__)
    ret = setreuid(ppw->pw_uid, ppw->pw_uid);
 #else
    ret = setresuid(ppw->pw_uid, ppw->pw_uid, 0);
@@ -2200,7 +2210,7 @@ ProcMgr_ImpersonateUserStop(void)
    // now group
 #if defined(USERWORLD)
    ret = Id_SetREGid(ppw->pw_gid, ppw->pw_gid);
-#elif defined(__APPLE__)
+#elif defined(__APPLE__) || defined(__NetBSD__)
    ret = setregid(ppw->pw_gid, ppw->pw_gid);
 #else
    ret = setresgid(ppw->pw_gid, ppw->pw_gid, ppw->pw_gid);
