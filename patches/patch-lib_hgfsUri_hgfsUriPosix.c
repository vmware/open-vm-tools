$NetBSD$

--- lib/hgfsUri/hgfsUriPosix.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/hgfsUri/hgfsUriPosix.c
@@ -23,7 +23,7 @@
  *    x-vmware-share:// style URIs
  */
 
-#if !defined __linux__ && !defined __APPLE__ && !defined __FreeBSD__
+#if !defined __linux__ && !defined __APPLE__ && !defined __FreeBSD__ && !defined __NetBSD__
 #   error This file should not be compiled
 #endif
 
