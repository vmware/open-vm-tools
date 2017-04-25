$NetBSD$

--- lib/xdg/xdg.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/xdg/xdg.c
@@ -91,7 +91,7 @@ Xdg_DetectDesktopEnv(void)
             int i;
 
             for (i = 0; i < outLen; i++) {
-               if (!isalnum(outbuf[i])) {
+               if (!isalnum((int) outbuf[i])) {
                   g_debug("%s: received malformed input\n", __func__);
                   free(outbuf);
                   outbuf = NULL;
