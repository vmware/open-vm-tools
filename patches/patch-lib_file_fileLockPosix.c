$NetBSD$

--- lib/file/fileLockPosix.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/file/fileLockPosix.c
@@ -32,6 +32,12 @@
 #include <sys/mount.h>
 #include <sys/times.h>
 #include <sys/sysctl.h>
+#elif defined(__NetBSD__)
+#include <sys/param.h>
+#include <sys/mount.h>
+#include <sys/times.h>
+#include <sys/sysctl.h> 
+#include <sys/statvfs.h>
 #else
 #include <sys/vfs.h>
 #endif
@@ -97,7 +103,7 @@
 static Bool
 IsLinkingAvailable(const char *fileName)  // IN:
 {
-   struct statfs buf;
+   struct statvfs buf;
    int status;
 
    ASSERT(fileName);
@@ -111,7 +117,7 @@ IsLinkingAvailable(const char *fileName)
       return FALSE;
    }
 
-   status = statfs(fileName, &buf);
+   status = statvfs(fileName, &buf);
 
    if (status == -1) {
       Log(LGPFX" Bad statfs using %s (%s).\n", fileName,
@@ -120,7 +126,7 @@ IsLinkingAvailable(const char *fileName)
       return FALSE;
    }
 
-#if defined(__APPLE__)
+#if defined(__APPLE__) || defined(__NetBSD__)
    if ((Str_Strcasecmp(buf.f_fstypename, "hfs") == 0) ||
        (Str_Strcasecmp(buf.f_fstypename, "nfs") == 0) ||
        (Str_Strcasecmp(buf.f_fstypename, "ufs") == 0)) {
