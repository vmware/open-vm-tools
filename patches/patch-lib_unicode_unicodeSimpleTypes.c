$NetBSD$

--- lib/unicode/unicodeSimpleTypes.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/unicode/unicodeSimpleTypes.c
@@ -2432,7 +2432,7 @@ UnicodeNormalizeEncodingName(const char 
    for (currentResult = result; *encodingName != '\0'; encodingName++) {
       // The explicit cast from char to int is necessary for Netware builds.
       if (isalnum((int) *encodingName)) {
-         *currentResult = tolower(*encodingName);
+         *currentResult = tolower((int) *encodingName);
          currentResult++;
       }
    }
