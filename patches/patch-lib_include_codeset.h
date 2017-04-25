$NetBSD$

--- lib/include/codeset.h.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/include/codeset.h
@@ -64,6 +64,7 @@
 #if defined(__FreeBSD__) || \
     defined(VMX86_SERVER) || \
     defined(__APPLE__) || \
+    defined(__NetBSD__) || \
     defined __ANDROID__
 #define CURRENT_IS_UTF8
 #endif
