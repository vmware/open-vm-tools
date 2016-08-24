$NetBSD$

--- lib/wiper/wiperPosix.c.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/wiper/wiperPosix.c
@@ -23,7 +23,7 @@
  *
  */
 
-#if !defined(__linux__) && !defined(sun) && !defined(__FreeBSD__) && !defined(__APPLE__)
+#if !defined(__NetBSD__) && !defined(__linux__) && !defined(sun) && !defined(__FreeBSD__) && !defined(__APPLE__)
 #error This file should not be compiled on this platform.
 #endif
 
@@ -31,7 +31,7 @@
 #include <sys/stat.h>
 #if defined(__linux__) || defined(sun)
 # include <sys/vfs.h>
-#elif defined(__FreeBSD__) || defined(__APPLE__)
+#elif defined(__NetBSD__) || defined(__FreeBSD__) || defined(__APPLE__)
 # include <sys/param.h>
 # include <sys/ucred.h>
 # include <sys/mount.h>
@@ -41,6 +41,10 @@
 # endif /* __FreeBSD_version >= 500000 */
 #endif
 #include <unistd.h>
+#if defined(__NetBSD__)
+#include <sys/statvfs.h>
+#endif
+
 
 #include "vmware.h"
 #include "wiper.h"
@@ -72,7 +76,7 @@
 
 #if defined(sun) || defined(__linux__)
 # define PROCFS "proc"
-#elif defined(__FreeBSD__) || defined(__APPLE__)
+#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__)
 # define PROCFS "procfs"
 #endif
 
@@ -145,6 +149,7 @@ static const PartitionInfo gKnownPartiti
    { "vfat",      PARTITION_FAT,          NULL,                   TRUE        },
    { "zfs",       PARTITION_ZFS,          NULL,                   FALSE       },
    { "xfs",       PARTITION_XFS,          NULL,                   TRUE        },
+   { "ffs",       PARTITION_FFS,          NULL,                   TRUE        },
    { "btrfs",     PARTITION_BTRFS,        NULL,                   TRUE        },
 };
 
@@ -328,7 +333,7 @@ WiperIsDiskDevice(MNTINFO *mnt,         
    return FALSE;
 }
 
-#elif defined(__FreeBSD__) /* } FreeBSD { */
+#elif defined (__NetBSD__) || defined(__FreeBSD__) /* } FreeBSD { */
 
 static Bool
 WiperIsDiskDevice(MNTINFO *mnt,         // IN: file system being considered
@@ -571,16 +576,16 @@ WiperSinglePartition_GetSpace(const Wipe
                               uint64 *free,       // OUT
                               uint64 *total)      // OUT
 {
-#ifdef sun
+#if defined(sun) || defined(__NetBSD__)
    struct statvfs statfsbuf;
 #else
-   struct statfs statfsbuf;
+   struct statvfs statfsbuf;
 #endif
    uint64 blockSize;
 
    ASSERT(p);
 
-#ifdef sun
+#if defined(sun) || defined(__NetBSD__)
    if (statvfs(p->mountPoint, &statfsbuf) < 0) {
 #else
    if (Posix_Statfs(p->mountPoint, &statfsbuf) < 0) {
@@ -588,7 +593,7 @@ WiperSinglePartition_GetSpace(const Wipe
       return "Unable to statfs() the mount point";
    }
 
-#ifdef sun
+#if defined(sun) || defined(__NetBSD__)
    blockSize = statfsbuf.f_frsize;
 #else
    blockSize = statfsbuf.f_bsize;
