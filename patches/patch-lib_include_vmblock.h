$NetBSD$

--- lib/include/vmblock.h.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/include/vmblock.h
@@ -83,11 +83,11 @@
 #ifndef _VMBLOCK_H_
 #define _VMBLOCK_H_
 
-#if defined(sun) || defined(__FreeBSD__)
+#if defined(sun) || defined(__FreeBSD__) || defined(__NetBSD__)
 # include <sys/ioccom.h>
 #endif
 
-#if defined(__FreeBSD__)
+#if defined(__FreeBSD__) || defined(__NetBSD__)
 # include <sys/param.h>
 #endif
 
@@ -164,7 +164,7 @@
                                        "/" VMBLOCK_CONTROL_DEVNAME
 # define VMBLOCK_DEVICE_MODE            O_WRONLY
 
-#elif defined(sun) || defined(__FreeBSD__)
+#elif defined(sun) || defined(__FreeBSD__) || defined(__NetBSD__)
 # define VMBLOCK_FS_NAME                "vmblock"
 # define VMBLOCK_MOUNT_POINT            "/var/run/" VMBLOCK_FS_NAME
 # define VMBLOCK_FS_ROOT                VMBLOCK_MOUNT_POINT
@@ -182,7 +182,7 @@
 #   define VMBLOCK_LIST_FILEBLOCKS       _IO('v', 3)
 #  endif
 
-# elif defined(__FreeBSD__)              /* } else if (FreeBSD) { */
+# elif defined(__FreeBSD__) || defined(__NetBSD__)              /* } else if (FreeBSD) { */
    /*
     * Similar to Solaris, construct ioctl(2) commands for block operations.
     * Since the FreeBSD implementation does not change the user's passed-in
