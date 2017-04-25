$NetBSD$

--- lib/impersonate/impersonatePosix.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/impersonate/impersonatePosix.c
@@ -279,7 +279,7 @@ ImpersonateUndo(void)
       goto exit;
    }
 
-#if __APPLE__
+#if __APPLE__ || __NetBSD__
    NOT_IMPLEMENTED();
 #else
    /* Return to root */
@@ -289,10 +289,14 @@ ImpersonateUndo(void)
    }
 #endif
 
+#if __NetBSD__
+  NOT_IMPLEMENTED();
+#else
    ret = Id_SetGid(ppw->pw_gid);
    if (ret < 0) {
       goto exit;
    }
+#endif
 
    /* 
     * The call to initgroups leaks memory in versions of glibc earlier than 2.1.93.
@@ -362,11 +366,15 @@ ImpersonateDoPosix(struct passwd *pwd)  
    ASSERT(getuid() == 0);
    VERIFY(geteuid() == 0);
 
+#if __NetBSD__
+  NOT_IMPLEMENTED();
+#else
    ret = Id_SetGid(pwd->pw_gid);
    if (ret < 0) {
       goto exit;
    }
-   
+#endif
+
    /* 
     * The call to initgroups leaks memory in versions of glibc earlier than
     * 2.1.93.See bug 10042. -jhu 
@@ -377,7 +385,7 @@ ImpersonateDoPosix(struct passwd *pwd)  
       goto exit;
    }
 
-#if __APPLE__
+#if __APPLE__ || __NetBSD__
    NOT_IMPLEMENTED();
 #else
    ret = Id_SetEUid(pwd->pw_uid);
