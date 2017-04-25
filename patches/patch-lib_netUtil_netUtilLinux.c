$NetBSD$

--- lib/netUtil/netUtilLinux.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/netUtil/netUtilLinux.c
@@ -31,7 +31,7 @@
 #endif
 
 
-#if !defined(__linux__) && !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
+#if !defined (__NetBSD__) && !defined(__linux__) && !defined(__FreeBSD__) && !defined(sun) && !defined(__APPLE__)
 #   error This file should not be compiled
 #endif
 
@@ -59,7 +59,7 @@
 #include <sys/ioctl.h>
 #include <net/if_arp.h>         // for ARPHRD_ETHER
 
-#if defined(__FreeBSD__) || defined(__APPLE__)
+#if defined(__FreeBSD__) || defined(__APPLE__) || defined (__NetBSD__)
 #include "ifaddrs.h"
 #endif
 
@@ -150,7 +150,7 @@ invalid:
  *----------------------------------------------------------------------
  */
 
-#if !defined(__FreeBSD__) && !defined(__APPLE__) /* { */
+#if !defined(__FreeBSD__) && !defined(__APPLE__) && !defined (__NetBSD__) /* { */
 char *
 NetUtil_GetPrimaryIP(void)
 {
