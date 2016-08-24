$NetBSD$

--- lib/hgfsHelper/hgfsHelperPosix.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/hgfsHelper/hgfsHelperPosix.c
@@ -24,7 +24,7 @@
  *
  */
 
-#if !defined __linux__ && !defined __FreeBSD__ && !defined sun && !defined __APPLE__
+#if !defined __linux__ && !defined __FreeBSD__ && !defined __NetBSD__ && !defined sun && !defined __APPLE__
 #   error This file should not be compiled
 #endif
 
@@ -65,7 +65,7 @@
 Bool
 HgfsHlpr_QuerySharesDefaultRootPath(char **hgfsRootPath)
 {
-#if defined __FreeBSD__
+#if defined __FreeBSD__ || defined __NetBSD__
    return FALSE;
 #else
    ASSERT(hgfsRootPath != NULL);
