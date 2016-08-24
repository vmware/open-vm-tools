$NetBSD$

--- lib/include/util.h.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/include/util.h
@@ -67,6 +67,9 @@
 #elif defined(__FreeBSD__)
 #  include <pthread.h>
    typedef pthread_t Util_ThreadID;
+#elif defined(__NetBSD__)
+#  include <pthread.h>
+   typedef pthread_t Util_ThreadID;
 #else
 #  error "Need typedef for Util_ThreadID"
 #endif
