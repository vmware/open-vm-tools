$NetBSD$

--- lib/misc/posixPosix.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/misc/posixPosix.c
@@ -47,7 +47,7 @@
 #include <sys/param.h>
 #include <sys/mount.h>
 #include <CoreFoundation/CoreFoundation.h>
-#elif defined(__FreeBSD__)
+#elif defined(__FreeBSD__) || defined(__NetBSD__)
 #include <sys/param.h>
 #include <sys/mount.h>
 #elif defined(sun)
@@ -59,6 +59,10 @@
 #include <mntent.h>
 #endif
 
+#if defined(__NetBSD__)
+#include <sys/statvfs.h>
+#endif
+
 #if (!defined(__FreeBSD__) || __FreeBSD_release >= 503001) && !defined __ANDROID__
 #define VM_SYSTEM_HAS_GETPWNAM_R 1
 #define VM_SYSTEM_HAS_GETPWUID_R 1
@@ -1606,7 +1610,7 @@ Posix_Putenv(char *name)  // IN:
 
 int
 Posix_Statfs(const char *pathName,      // IN:
-             struct statfs *statfsbuf)  // IN:
+             struct statvfs *statfsbuf)  // IN:
 {
    char *path;
    int ret;
@@ -1615,7 +1619,7 @@ Posix_Statfs(const char *pathName,      
       return -1;
    }
 
-   ret = statfs(path, statfsbuf);
+   ret = statvfs(path, statfsbuf);
 
    free(path);
 
@@ -1746,7 +1750,7 @@ Posix_Unsetenv(const char *name)  // IN:
 
 #if !defined(sun) // {
 
-#if !defined(__APPLE__) && !defined(__FreeBSD__) // {
+#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined (__NetBSD__) // {
 /*
  *----------------------------------------------------------------------
  *
