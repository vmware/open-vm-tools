$NetBSD$

--- lib/include/su.h.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/include/su.h
@@ -32,7 +32,7 @@
 #include "vm_basic_types.h"
 #include "vm_assert.h"
 
-#if defined(__APPLE__)
+#if (defined(__APPLE__) || defined(__NetBSD__))
 
 #include <sys/types.h>
 #include <unistd.h>
