$NetBSD$

--- lib/misc/hostinfoPosix.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/misc/hostinfoPosix.c
@@ -40,7 +40,7 @@
 #include <sys/systeminfo.h>
 #endif
 #include <sys/socket.h>
-#if defined(__FreeBSD__) || defined(__APPLE__)
+#if defined(__FreeBSD__) || defined(__APPLE__) || defined(__NetBSD__)
 # include <sys/sysctl.h>
 #endif
 #if !defined(__APPLE__)
@@ -87,7 +87,7 @@
 #endif
 #endif
 
-#if defined(__APPLE__) || defined(__FreeBSD__)
+#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)
 #include <paths.h>
 #endif
 
@@ -290,7 +290,7 @@ HostinfoOSVersionInit(void)
     */
 
    p = extra;
-   while (*p && !isdigit(*p)) {
+   while (*p && !isdigit((int) *p)) {
       p++;
    }
    sscanf(p, "%d", &version->hostinfoOSVersion[3]);
@@ -2441,8 +2441,8 @@ HostinfoGetCpuInfo(int nCpu,    // IN:
          e = s + strlen(s);
 
          /* Skip leading and trailing while spaces */
-         for (; s < e && isspace(*s); s++);
-         for (; s < e && isspace(e[-1]); e--);
+         for (; s < e && isspace((int) *s); s++);
+         for (; s < e && isspace((int) e[-1]); e--);
          *e = 0;
 
          /* Free previous value */
@@ -3289,7 +3289,8 @@ HostinfoSysinfo(uint64 *totalRam,  // OU
 #endif // ifndef __APPLE__
 
 
-#if defined(__linux__) || defined(__FreeBSD__) || defined(sun)
+// To Fix for NetBSD
+#if defined(__linux__) || defined(__FreeBSD__) || defined(sun) || defined(__NetBSD__)
 /*
  *-----------------------------------------------------------------------------
  *
