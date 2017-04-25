$NetBSD$

--- lib/misc/strutil.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/misc/strutil.c
@@ -1211,7 +1211,7 @@ StrUtil_TrimWhitespace(const char *str) 
    size_t len;
 
    /* Skip leading whitespace. */
-   while (*cur && isspace(*cur)) {
+   while (*cur && isspace((int) *cur)) {
       cur++;
    }
 
@@ -1225,7 +1225,7 @@ StrUtil_TrimWhitespace(const char *str) 
    }
 
    cur = res + len - 1;
-   while (cur > res && isspace(*cur)) {
+   while (cur > res && isspace((int) *cur)) {
       cur--;
    }
 
