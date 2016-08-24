$NetBSD$

--- lib/include/hgfsUtil.h.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/include/hgfsUtil.h
@@ -58,6 +58,7 @@
        !defined __timespec_defined && \
        !defined sun && \
        !defined __FreeBSD__ && \
+       !defined __NetBSD__ && \
        !__APPLE__ && \
        !defined _WIN32
 struct timespec {
