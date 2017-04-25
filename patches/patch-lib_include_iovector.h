$NetBSD$

--- lib/include/iovector.h.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/include/iovector.h
@@ -32,7 +32,7 @@
 /*
  * Ugly definition of struct iovec.
  */
-#if defined(__linux__) || defined(sun) || defined(__APPLE__) || defined(__FreeBSD__)
+#if defined(__linux__) || defined(sun) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)
 #include <sys/uio.h>    // for struct iovec
 #else
 
