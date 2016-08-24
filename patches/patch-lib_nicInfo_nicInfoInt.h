$NetBSD$

--- lib/nicInfo/nicInfoInt.h.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/nicInfo/nicInfoInt.h
@@ -29,7 +29,7 @@
 
 #include "nicInfo.h"
 
-#if defined __FreeBSD__ || defined __sun__ || defined __APPLE__
+#if defined __FreeBSD__ || defined __sun__ || defined __APPLE__ || defined __NetBSD__
 #   include <sys/socket.h>      // struct sockaddr
 #endif
 
