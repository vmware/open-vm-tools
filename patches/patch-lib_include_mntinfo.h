$NetBSD$

--- lib/include/mntinfo.h.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/include/mntinfo.h
@@ -37,6 +37,10 @@
 #elif defined(__FreeBSD__)
 # include <sys/mount.h>
 #endif
+#if defined(__NetBSD__)
+# include <sys/mount.h>
+#include <sys/statvfs.h>
+#endif
 #include "posix.h"
 
 /*
@@ -106,17 +110,17 @@
 # define MNTINFO_FSTYPE(mnt)            mnt->mnt_type
 # define MNTINFO_MNTPT(mnt)             mnt->mnt_dir
 # define MNTINFO_MNT_IS_RO(mnt)         (hasmntopt((mnt), "rw") == NULL)
-#elif defined(__FreeBSD__) || defined(__APPLE__)
+#elif defined(__NetBSD__) || defined(__FreeBSD__) || defined(__APPLE__)
 struct mntHandle {
-   struct statfs *mountPoints;  // array of mountpoints per getmntinfo(3)
+   struct statvfs *mountPoints;  // array of mountpoints per getmntinfo(3)
    int numMountPoints;          // number of elements in mntArray
    int mountIndex;              // current location within mountPoints array
 };
 # define MNTFILE                        _PATH_FSTAB
 # define MNTHANDLE                      struct mntHandle *
-# define MNTINFO                        struct statfs
-# define DECLARE_MNTINFO(name)          struct statfs __ ## name; \
-                                        struct statfs *name = &__ ## name
+# define MNTINFO                        struct statvfs
+# define DECLARE_MNTINFO(name)          struct statvfs __ ## name; \
+                                        struct statvfs *name = &__ ## name
 
 # define OPEN_MNTFILE(mode)                                             \
 ({                                                                      \
@@ -155,7 +159,7 @@ struct mntHandle {
 # define MNTINFO_NAME(mnt)              mnt->f_mntfromname
 # define MNTINFO_FSTYPE(mnt)            mnt->f_fstypename
 # define MNTINFO_MNTPT(mnt)             mnt->f_mntonname
-# define MNTINFO_MNT_IS_RO(mnt)         ((mnt)->f_flags & MNT_RDONLY)
+# define MNTINFO_MNT_IS_RO(mnt)         ((mnt)->f_flag & MNT_RDONLY)
 #else
 # error "Define mount information macros for your OS type"
 #endif
