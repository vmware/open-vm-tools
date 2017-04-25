$NetBSD$

--- lib/dataMap/dataMap.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/dataMap/dataMap.c
@@ -1021,7 +1021,7 @@ IsPrintable(const char *str,    // IN
 
    for (cc = 0; cc < strLen; cc ++) {
       /* isprint crashes with negative value in windows debug mode */
-      if ((!CType_IsPrint(str[cc])) && (!CType_IsSpace(str[cc]))) {
+      if ((!CType_IsPrint((int) str[cc])) && (!CType_IsSpace((int) str[cc]))) {
          printable = FALSE;
          break;
       }
