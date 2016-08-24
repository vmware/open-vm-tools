$NetBSD$

--- lib/misc/util_misc.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/misc/util_misc.c
@@ -479,7 +479,7 @@ Util_GetCurrentThreadId(void)
     * would take two syscalls instead of zero.
     */
    return pthread_mach_thread_np(pthread_self());
-#elif defined(__FreeBSD__)
+#elif defined(__FreeBSD__) || defined(__NetBSD__)
    /*
     * These OSes do not implement OS-native thread IDs. You probably
     * didn't need one anyway, but guess that pthread_self works
