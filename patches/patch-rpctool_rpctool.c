$NetBSD$

--- rpctool/rpctool.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ rpctool/rpctool.c
@@ -27,6 +27,9 @@
 #include <errno.h>
 #include <stdint.h>
 #endif
+#ifdef __NetBSD__
+#include "sigPosixRegs.h"
+#endif
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
