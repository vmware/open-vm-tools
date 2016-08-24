$NetBSD$

--- lib/misc/idLinux.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/misc/idLinux.c
@@ -119,7 +119,7 @@ static AuthorizationRef IdAuthCreateWith
 int
 Id_SetUid(uid_t euid)		// IN: new euid
 {
-#if defined(__FreeBSD__) || defined(sun)
+#if defined(__FreeBSD__) || defined(sun) || defined(__NetBSD__)
    return setuid(euid);
 #elif defined(linux) || defined __ANDROID__
    if (uid32) {
@@ -162,7 +162,7 @@ Id_SetGid(gid_t egid)		// IN: new egid
    Warning("XXXMACOS: implement %s\n", __func__);
 
    return -1;
-#elif defined(sun) || defined(__FreeBSD__)
+#elif defined(sun) || defined(__FreeBSD__) || defined(__NetBSD__)
    return setgid(egid);
 #else
    if (uid32) {
@@ -365,7 +365,7 @@ Id_SetREUid(uid_t uid,		// IN: new uid
 #if defined(__APPLE__)
    Warning("XXXMACOS: implement %s\n", __func__);
    return -1;
-#elif defined(sun) || defined(__FreeBSD__)
+#elif defined(sun) || defined(__FreeBSD__) || defined(__NetBSD__)
    return setreuid(uid, euid);
 #else
    if (uid32) {
@@ -406,7 +406,7 @@ int
 Id_SetREGid(gid_t gid,		// IN: new gid
 	    gid_t egid)		// IN: new effective gid
 {
-#if defined(sun) || defined(__FreeBSD__)
+#if defined(sun) || defined(__FreeBSD__) || defined(__NetBSD__)
    return setregid(gid, egid);
 #else
    if (uid32) {
