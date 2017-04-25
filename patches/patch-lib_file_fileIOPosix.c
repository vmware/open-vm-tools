$NetBSD$

--- lib/file/fileIOPosix.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/file/fileIOPosix.c
@@ -66,7 +66,7 @@
 #include <dlfcn.h>
 #include <sys/xattr.h>
 #else
-#if defined(__FreeBSD__)
+#if defined(__FreeBSD__) || defined(__NetBSD__)
 #include <sys/param.h>
 #include <sys/mount.h>
 #else
