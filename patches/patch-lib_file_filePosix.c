$NetBSD$

--- lib/file/filePosix.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/file/filePosix.c
@@ -28,15 +28,17 @@
 # include <sys/param.h>
 # include <sys/mount.h>
 #else
-# if !defined(__APPLE__)
+# if !defined(__APPLE__) && !defined(__NetBSD__)
 #  include <sys/vfs.h>
 # endif
 # include <limits.h>
 # include <stdio.h>      /* Needed before sys/mnttab.h in Solaris */
 # if defined(sun)
 #  include <sys/mnttab.h>
-# elif __APPLE__
+# elif defined(__APPLE__)
 #  include <sys/mount.h>
+# elif defined(__NetBSD__)
+#  include <sys/statvfs.h>
 # else
 #  include <mntent.h>
 # endif
@@ -74,7 +76,7 @@
 #include "unicodeOperations.h"
 
 #if !defined(__FreeBSD__) && !defined(sun)
-#if !defined(__APPLE__)
+#if !defined(__APPLE__) && !defined(__NetBSD__)
 static char *FilePosixLookupMountPoint(char const *canPath, Bool *bind);
 #endif
 static char *FilePosixNearestExistingAncestor(char const *path);
@@ -364,6 +366,7 @@ FileAttributes(const char *pathName,  //
 Bool
 File_IsRemote(const char *pathName)  // IN: Path name
 {
+   struct statvfs sfbuf;
    if (HostType_OSIsVMK()) {
       /*
        * All files and file systems are treated as "directly attached"
@@ -372,16 +375,14 @@ File_IsRemote(const char *pathName)  // 
 
       return FALSE;
    } else {
-      struct statfs sfbuf;
-
       if (Posix_Statfs(pathName, &sfbuf) == -1) {
-         Log(LGPFX" %s: statfs(%s) failed: %s\n", __func__, pathName,
+         Log(LGPFX" %s: statfs12(%s) failed: %s\n", __func__, pathName,
              Err_Errno2String(errno));
 
          return TRUE;
       }
-#if defined(__APPLE__)
-      return sfbuf.f_flags & MNT_LOCAL ? FALSE : TRUE;
+#if defined(__APPLE__) || defined(__NetBSD__)
+      return sfbuf.f_flag & MNT_LOCAL ? FALSE : TRUE;
 #else
       if (NFS_SUPER_MAGIC == sfbuf.f_type) {
          return TRUE;
@@ -1015,7 +1016,7 @@ File_GetParent(char **canPath)  // IN/OU
 static Bool
 FileGetStats(const char *pathName,       // IN:
              Bool doNotAscend,           // IN:
-             struct statfs *pstatfsbuf)  // OUT:
+             struct statvfs *pstatfsbuf)  // OUT:
 {
    Bool retval = TRUE;
    char *dupPath = NULL;
@@ -1066,7 +1067,7 @@ File_GetFreeSpace(const char *pathName, 
 {
    uint64 ret;
    char *fullPath;
-   struct statfs statfsbuf;
+   struct statvfs statfsbuf;
 
    fullPath = File_FullPath(pathName);
    if (fullPath == NULL) {
@@ -1074,7 +1075,7 @@ File_GetFreeSpace(const char *pathName, 
    }
 
    if (FileGetStats(fullPath, doNotAscend, &statfsbuf)) {
-      ret = (uint64) statfsbuf.f_bavail * statfsbuf.f_bsize;
+      ret = (uint64) statfsbuf.f_bavail * statfsbuf.f_frsize;
    } else {
       Warning("%s: Couldn't statfs %s\n", __func__, fullPath);
       ret = -1;
@@ -1518,7 +1519,7 @@ File_GetCapacity(const char *pathName)  
 {
    uint64 ret;
    char *fullPath;
-   struct statfs statfsbuf;
+   struct statvfs statfsbuf;
 
    fullPath = File_FullPath(pathName);
    if (fullPath == NULL) {
@@ -1526,7 +1527,7 @@ File_GetCapacity(const char *pathName)  
    }
 
    if (FileGetStats(fullPath, FALSE, &statfsbuf)) {
-      ret = (uint64) statfsbuf.f_blocks * statfsbuf.f_bsize;
+      ret = (uint64) statfsbuf.f_blocks * statfsbuf.f_frsize;
    } else {
       Warning(LGPFX" %s: Couldn't statfs\n", __func__);
       ret = -1;
@@ -1641,7 +1642,7 @@ exit:
 }
 
 
-#if !defined(__APPLE__)
+#if !defined(__APPLE__) && !defined(__NetBSD__)
 /*
  *-----------------------------------------------------------------------------
  *
@@ -1789,8 +1790,8 @@ FilePosixGetBlockDevice(char const *path
 {
    char *existPath;
    Bool failed;
-#if defined(__APPLE__)
-   struct statfs buf;
+#if defined(__APPLE__) || defined(__NetBSD__)
+   struct statvfs buf;
 #else
    char canPath[FILE_MAXPATH];
    char canPath2[FILE_MAXPATH];
@@ -1800,8 +1801,8 @@ FilePosixGetBlockDevice(char const *path
 
    existPath = FilePosixNearestExistingAncestor(path);
 
-#if defined(__APPLE__)
-   failed = statfs(existPath, &buf) == -1;
+#if defined(__APPLE__) || defined(__NetBSD__)
+   failed = statvfs(existPath, &buf) == -1;
    free(existPath);
    if (failed) {
       return NULL;
@@ -2007,8 +2008,8 @@ File_IsSameFile(const char *path1,  // I
    struct stat st1;
    struct stat st2;
 #if !defined(sun)  // Solaris does not have statfs
-   struct statfs stfs1;
-   struct statfs stfs2;
+   struct statvfs stfs1;
+   struct statvfs stfs2;
 #endif
 
    ASSERT(path1);
@@ -2058,8 +2059,8 @@ File_IsSameFile(const char *path1,  // I
       return FALSE;
    }
 
-#if defined(__APPLE__) || defined(__FreeBSD__)
-   if ((stfs1.f_flags & MNT_LOCAL) && (stfs2.f_flags & MNT_LOCAL)) {
+#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)
+   if ((stfs1.f_flag & MNT_LOCAL) && (stfs2.f_flag & MNT_LOCAL)) {
       return TRUE;
    }
 #else
