$NetBSD$

--- lib/system/systemLinux.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/system/systemLinux.c
@@ -26,7 +26,7 @@
  *
  */
 
-#if !defined(__linux__) && !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
+#if !defined(__NetBSD__) && !defined(__linux__) && !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
 #   error This file should not be compiled
 #endif
 
@@ -59,7 +59,11 @@
 #   include <utmpx.h>
 #endif
 
-#ifdef __FreeBSD__
+#ifdef __FreeBSD__ 
+#include "ifaddrs.h"
+#endif
+
+#if defined __NetBSD__
 #include "ifaddrs.h"
 #endif
 
@@ -366,7 +370,7 @@ System_Shutdown(Bool reboot)  // IN: "re
       cmd = "/sbin/shutdown -r now";
 #endif
    } else {
-#if __FreeBSD__
+#if __FreeBSD__ || __NetBSD__
       cmd = "/sbin/shutdown -p now";
 #elif defined(sun)
       cmd = "/usr/sbin/shutdown -g 0 -i 5 -y";
