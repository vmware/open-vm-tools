$NetBSD$

--- lib/include/wiper.h.orig	2015-11-24 07:07:44.000000000 +0000
+++ lib/include/wiper.h
@@ -48,6 +48,7 @@ typedef enum {
    PARTITION_HFS,
    PARTITION_ZFS,
    PARTITION_XFS,
+   PARTITION_FFS,
    PARTITION_BTRFS,
 } WiperPartition_Type;
 
