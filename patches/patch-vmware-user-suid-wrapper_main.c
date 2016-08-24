$NetBSD$

--- vmware-user-suid-wrapper/main.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ vmware-user-suid-wrapper/main.c
@@ -28,7 +28,7 @@
  *      remove blocks in the blocking file system.
  */
 
-#if !defined(sun) && !defined(__FreeBSD__) && !defined(linux)
+#if !defined(__NetBSD__) && !defined(sun) && !defined(__FreeBSD__) && !defined(linux)
 # error This program is not supported on your platform.
 #endif
 
