$NetBSD$

--- lib/include/posix.h.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/include/posix.h
@@ -63,7 +63,7 @@ struct stat;
 #if defined(_WIN32)
 typedef int mode_t;
 #else
-struct statfs;
+struct statvfs;
 struct utimbuf;
 struct timeval;
 struct passwd;
@@ -104,7 +104,7 @@ char *Posix_MkTemp(const char *pathName)
  * Make them NULL wrappers for all other platforms.
  */
 #define Posix_GetHostName gethostname
-#if defined(__APPLE__)
+#if defined(__APPLE__) || defined(__NetBSD__)
 #define Posix_GetHostByName gethostbyname
 #endif
 #define Posix_GetAddrInfo getaddrinfo
@@ -158,7 +158,7 @@ int Posix_Getgrnam_r(const char *name, s
                  char *buf, size_t size, struct group **pgr);
 
 #if !defined(sun)
-int Posix_Statfs(const char *pathName, struct statfs *statfsbuf);
+int Posix_Statfs(const char *pathName, struct statvfs *statfsbuf);
 
 int Posix_GetGroupList(const char *user, gid_t group, gid_t *groups,
                        int *ngroups);
@@ -178,7 +178,7 @@ struct mntent *Posix_Getmntent_r(FILE *f
 int Posix_Getmntent(FILE *fp, struct mnttab *mp);
 
 #endif // !defined(sun)
-#if !defined(__APPLE__)
+#if !defined(__APPLE__) && !defined(__NetBSD__)
 
 
 /*
@@ -804,7 +804,7 @@ exit:
 #define Posix_Setenv setenv
 #define Posix_Setmntent setmntent
 #define Posix_Stat stat
-#define Posix_Statfs statfs
+#define Posix_Statfs statvfs
 #define Posix_Symlink symlink
 #define Posix_System system
 #define Posix_Truncate truncate
